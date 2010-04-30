// Copyright (c) 2009 - Mozy, Inc.

#include "mordor/pch.h"

#include "oauth.h"

namespace Mordor {
namespace HTTP {
namespace OAuth {

static void writeBody(ClientRequest::ptr request, const std::string &body)
{
    request->requestStream()->write(body.c_str(), body.size());
    request->requestStream()->close();
}

static std::pair<std::string, std::string>
extractCredentials(ClientRequest::ptr request)
{
    URI::QueryString responseParams = request->responseStream();
    std::pair<std::string, std::string> result;
    std::pair<URI::QueryString::iterator, URI::QueryString::iterator> its =
        responseParams.equal_range("oauth_token");
    if (its.first == responseParams.end() || its.first->second.empty())
        MORDOR_THROW_EXCEPTION(InvalidResponseException("Missing oauth_token in response",
            request));
    result.first = its.first->second;
    ++its.first;
    if (its.first != its.second)
        MORDOR_THROW_EXCEPTION(InvalidResponseException("Duplicate oauth_token in response",
            request));
    its = responseParams.equal_range("oauth_token_secret");
    if (its.first == responseParams.end() || its.first->second.empty())
        MORDOR_THROW_EXCEPTION(InvalidResponseException("Missing oauth_token_secret in response",
            request));
    result.second = its.first->second;
    ++its.first;
    if (its.first != its.second)
        MORDOR_THROW_EXCEPTION(InvalidResponseException("Duplicate oauth_token_secret in response",
            request));
    return result;
}

std::pair<std::string, std::string>
getTemporaryCredentials(RequestBroker::ptr requestBroker, const URI &uri,
    Method method, const std::string &signatureMethod,
    const std::pair<std::string, std::string> &clientCredentials,
    const URI &callbackUri)
{
    MORDOR_ASSERT(requestBroker);
    MORDOR_ASSERT(uri.isDefined());
    MORDOR_ASSERT(method == GET || method == POST);
    MORDOR_ASSERT(signatureMethod == "PLAINTEXT" || signatureMethod == "HMAC-SHA1");
    MORDOR_ASSERT(!clientCredentials.first.empty());
    MORDOR_ASSERT(!clientCredentials.second.empty());
    URI::QueryString oauthParameters;

    oauthParameters.insert(std::make_pair("oauth_consumer_key", clientCredentials.first));
    oauthParameters.insert(std::make_pair("oauth_version", "1.0"));
    if (!callbackUri.isDefined())
        oauthParameters.insert(std::make_pair("oauth_callback", "oob"));
    else
        oauthParameters.insert(std::make_pair("oauth_callback", callbackUri.toString()));
    nonceAndTimestamp(oauthParameters);
    sign(uri, method, signatureMethod, clientCredentials.second, std::string(),
        oauthParameters);

    Request requestHeaders;
    requestHeaders.requestLine.method = method;
    requestHeaders.requestLine.uri = uri;
    std::string body;
    if (method == GET) {
        // Add parameters that are part of the request token URI
        URI::QueryString qsFromUri = uri.queryString();
        oauthParameters.insert(qsFromUri.begin(), qsFromUri.end());
        requestHeaders.requestLine.uri.query(oauthParameters);
    } else {
        body = oauthParameters.toString();
        requestHeaders.entity.contentType.type = "application";
        requestHeaders.entity.contentType.subtype = "x-www-form-urlencoded";
        requestHeaders.entity.contentLength = body.size();
    }

    boost::function<void (ClientRequest::ptr)> bodyDg;
    if (!body.empty())
        bodyDg = boost::bind(&writeBody, _1, boost::cref(body));
    ClientRequest::ptr request;
    try {
        request = requestBroker->request(requestHeaders, false, bodyDg);
    } catch (...) {
        throw;
    }
    if (request->response().status.status != OK)
        MORDOR_THROW_EXCEPTION(InvalidResponseException(request));

    return extractCredentials(request);
}

std::pair<std::string, std::string>
getTokenCredentials(RequestBroker::ptr requestBroker, const URI &uri,
    Method method, const std::string signatureMethod,
    const std::pair<std::string, std::string> &clientCredentials,
    const std::pair<std::string, std::string> &temporaryCredentials,
    const std::string &verifier)
{
    MORDOR_ASSERT(requestBroker);
    MORDOR_ASSERT(uri.isDefined());
    MORDOR_ASSERT(method == GET || method == POST);
    MORDOR_ASSERT(signatureMethod == "PLAINTEXT" || signatureMethod == "HMAC-SHA1");
    MORDOR_ASSERT(!clientCredentials.first.empty());
    MORDOR_ASSERT(!clientCredentials.second.empty());
    MORDOR_ASSERT(!temporaryCredentials.first.empty());
    MORDOR_ASSERT(!temporaryCredentials.second.empty());
    URI::QueryString oauthParameters;

    oauthParameters.insert(std::make_pair("oauth_consumer_key", clientCredentials.first));
    oauthParameters.insert(std::make_pair("oauth_token", temporaryCredentials.first));
    oauthParameters.insert(std::make_pair("oauth_verifier", verifier));
    oauthParameters.insert(std::make_pair("oauth_version", "1.0"));
    nonceAndTimestamp(oauthParameters);
    sign(uri, method, signatureMethod, clientCredentials.second,
        temporaryCredentials.second, oauthParameters);

    Request requestHeaders;
    requestHeaders.requestLine.method = method;
    requestHeaders.requestLine.uri = uri;
    std::string body;
    if (method == GET) {
        // Add parameters that are part of the request token URI
        URI::QueryString qsFromUri = uri.queryString();
        oauthParameters.insert(qsFromUri.begin(), qsFromUri.end());
        requestHeaders.requestLine.uri.query(oauthParameters);
    } else {
        body = oauthParameters.toString();
        requestHeaders.entity.contentType.type = "application";
        requestHeaders.entity.contentType.subtype = "x-www-form-urlencoded";
        requestHeaders.entity.contentLength = body.size();
    }

    boost::function<void (ClientRequest::ptr)> bodyDg;
    if (!body.empty())
        bodyDg = boost::bind(&writeBody, _1, boost::cref(body));
    ClientRequest::ptr request;
    try {
        request = requestBroker->request(requestHeaders, false, bodyDg);
    } catch (...) {
        throw;
    }
    if (request->response().status.status != OK)
        MORDOR_THROW_EXCEPTION(InvalidResponseException(request));

    return extractCredentials(request);
}

void
authorize(Request &nextRequest, const std::string &signatureMethod,
    const std::pair<std::string, std::string> &clientCredentials,
    const std::pair<std::string, std::string> &tokenCredentials,
    const std::string &realm)
{
    MORDOR_ASSERT(signatureMethod == "PLAINTEXT" || signatureMethod == "HMAC-SHA1");
    MORDOR_ASSERT(!clientCredentials.first.empty());
    MORDOR_ASSERT(!clientCredentials.second.empty());
    MORDOR_ASSERT(!tokenCredentials.first.empty());
    MORDOR_ASSERT(!tokenCredentials.second.empty());

    AuthParams &authorization = nextRequest.request.authorization;
    authorization.scheme = "OAuth";
    authorization.parameters["oauth_consumer_key"] = clientCredentials.first;
    authorization.parameters["oauth_token"] = tokenCredentials.first;
    authorization.parameters["oauth_version"] = "1.0";
    nonceAndTimestamp(authorization.parameters);
    sign(nextRequest.requestLine.uri, nextRequest.requestLine.method,
        signatureMethod, clientCredentials.second, tokenCredentials.second,
        authorization.parameters);
    if (!realm.empty())
        authorization.parameters["realm"] = realm;
}

template <class T>
void nonceAndTimestamp(T &oauthParameters)
{
    static boost::posix_time::ptime start(boost::gregorian::date(1970, 1, 1));
    static const char *allowedChars =
        "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::ostringstream os;
    boost::posix_time::ptime now =
        boost::posix_time::second_clock::universal_time();
    boost::posix_time::time_duration duration = now - start;
    os << duration.total_seconds();

    std::string nonce;
    nonce.resize(40);
    for (size_t i = 0; i < 40; ++i) {
        nonce[i] = allowedChars[rand() % 36];
    }

    typename T::iterator it = oauthParameters.find("oauth_timestamp");
    if (it != oauthParameters.end())
        oauthParameters.erase(it);
    it = oauthParameters.find("oauth_nonce");
    if (it != oauthParameters.end())
        oauthParameters.erase(it);
    oauthParameters.insert(std::make_pair("oauth_timestamp", os.str()));
    oauthParameters.insert(std::make_pair("oauth_nonce", nonce));
}

template void nonceAndTimestamp<StringMap>(StringMap &);
template void nonceAndTimestamp<URI::QueryString>(URI::QueryString &);

template <class T>
void
sign(const URI &uri, Method method, const std::string &signatureMethod,
    const std::string &clientSecret, const std::string &tokenSecret,
    T &oauthParameters, const URI::QueryString &postParameters)
{
    MORDOR_ASSERT(oauthParameters.find("oauth_signature_method") == oauthParameters.end());
    oauthParameters.insert(std::make_pair("oauth_signature_method", signatureMethod));
    MORDOR_ASSERT(oauthParameters.find("oauth_signature") == oauthParameters.end());

    typename T::iterator it;

    std::ostringstream os;
    URI requestUri(uri);
    requestUri.queryDefined(false);
    requestUri.fragmentDefined(false);
    requestUri.normalize();
    os << method << '&' << URI::encode(requestUri.toString());
    std::map<std::string, std::multiset<std::string> > combined;
    std::map<std::string, std::multiset<std::string> >::iterator
        combinedIt;
    for (it = oauthParameters.begin(); it != oauthParameters.end(); ++it)
        if (stricmp(it->first.c_str(), "realm") != 0)
            combined[it->first].insert(it->second);
    URI::QueryString::const_iterator it2;
    for (it2 = postParameters.begin(); it2 != postParameters.end(); ++it2)
        combined[it2->first].insert(it2->second);
    if (uri.queryDefined()) {
        URI::QueryString queryParams = uri.queryString();
        for (it2 = queryParams.begin(); it2 != queryParams.end(); ++it2)
            combined[it2->first].insert(it2->second);
    }

    os << '&';
    std::string signatureBaseString = os.str();
    os.str("");
    bool first = true;
    for (combinedIt = combined.begin();
        combinedIt != combined.end();
        ++combinedIt) {
        for (std::multiset<std::string>::iterator it2 =
            combinedIt->second.begin();
            it2 != combinedIt->second.end();
            ++it2) {
            if (!first)
                os << '&';
            first = false;
            os << URI::encode(combinedIt->first)
                << '=' << URI::encode(*it2);
        }
    }
    signatureBaseString.append(URI::encode(os.str()));

    std::string secrets = URI::encode(clientSecret);
    secrets.append(1, '&');
    secrets.append(URI::encode(tokenSecret));

    if (stricmp(signatureMethod.c_str(), "HMAC-SHA1") == 0) {
        oauthParameters.insert(std::make_pair("oauth_signature",
            base64encode(hmacSha1(signatureBaseString, secrets))));
    } else if (stricmp(signatureMethod.c_str(), "PLAINTEXT") == 0) {
        oauthParameters.insert(std::make_pair("oauth_signature", secrets));
    } else {
        MORDOR_NOTREACHED();
    }
}

template void sign<StringMap>(const URI &, Method, const std::string &,
    const std::string &, const std::string &, StringMap &,
    const URI::QueryString &);
template void sign<URI::QueryString>(const URI &, Method, const std::string &,
    const std::string &, const std::string &, URI::QueryString &,
    const URI::QueryString &);

ClientRequest::ptr
RequestBroker::request(Request &requestHeaders, bool forceNewConnection,
    boost::function<void (ClientRequest::ptr)> bodyDg)
{
    ClientRequest::ptr priorRequest;
    std::pair<std::string, std::string> clientCredentials, tokenCredentials;
    std::string signatureMethod, realm;
    while (true) {
        if (m_getCredentialsDg(requestHeaders.requestLine.uri, priorRequest,
            signatureMethod, clientCredentials, tokenCredentials, realm))
            authorize(requestHeaders, signatureMethod, clientCredentials,
                tokenCredentials, realm);
        if (priorRequest)
            priorRequest->finish();
        priorRequest = parent()->request(requestHeaders, forceNewConnection,
            bodyDg);
        if (priorRequest->response().status.status == UNAUTHORIZED) {
            if (isAcceptable(priorRequest->response().response.wwwAuthenticate,
                "OAuth"))
                continue;
        }
        return priorRequest;
    }
}

}}}
