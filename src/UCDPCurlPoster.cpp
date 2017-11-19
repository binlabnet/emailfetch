/*
 * CurlPoster.cpp
 *
 *  Created on: Oct 28, 2017
 *      Author: paulc
 */
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string>
#include <cstddef>
#include <limits.h>
#include <iostream>
#include <iomanip>
#include <errno.h>
#include <fstream>
#include <ios>
#include <sstream>
#include "UCDPCurlPoster.h"
using namespace std;

namespace S3Downloader
{

UCDPCurlPoster::UCDPCurlPoster(std::string url) :
    m_url(url), m_curl(NULL), m_opts(NULL), m_resolve(NULL), m_curl_status(CURLE_OK)
{
    init();
}

UCDPCurlPoster::UCDPCurlPoster(string hostname, unsigned short port, string ip, string certificate, string password) :
    m_curl(NULL), m_opts(NULL), m_resolve(NULL), m_curl_status(CURLE_OK)
{
    init();
    set_ServerNameIndication(hostname, port, ip);
    set_Certificate(certificate, password);
}

void UCDPCurlPoster::setProxy(std::string proxy)
{
    if (m_curl)
        m_curl_status = curl_easy_setopt(m_curl, CURLOPT_PROXY, proxy.c_str());

}

void UCDPCurlPoster::set_ServerNameIndication(string hostname, unsigned short port, string ip)
{
    if (m_curl)
    {
        ostringstream r_out;
        r_out << hostname << ":" << port << ":" << ip;
        string resolve = r_out.str();
        if (m_resolve)
        {
            curl_slist_free_all(m_resolve);
            m_resolve = NULL;
        }

        ostringstream u_out;
        u_out << "https://" << hostname << ":" << port << "/ucdpext/post";
        m_url = u_out.str();

        m_resolve = curl_slist_append(NULL, resolve.c_str());
        if (m_resolve)
        {
            if ((m_curl_status = curl_easy_setopt(m_curl, CURLOPT_RESOLVE, m_resolve)) != CURLE_OK)
                return;

            if ((m_curl_status = curl_easy_setopt(m_curl, CURLOPT_URL, m_url.c_str())) != CURLE_OK)
                return;
        }
        else
        {
            m_curl_status = CURLE_FAILED_INIT;
            //cout << "Error: Unable to set resolve list for server name indication" << endl;
        }
    }
}

void UCDPCurlPoster::set_Certificate(string certificate, string password)
{
    if ((m_curl_status = curl_easy_setopt(m_curl, CURLOPT_SSLCERTTYPE, "PEM")) != CURLE_OK)
        return;

    if ((m_curl_status = curl_easy_setopt(m_curl, CURLOPT_KEYPASSWD, password.c_str())) != CURLE_OK)
        return;

    if ((m_curl_status = curl_easy_setopt(m_curl, CURLOPT_SSLCERT, certificate.c_str())) != CURLE_OK)
        return;

    // don't verify peer as UCDP uses self-signed certificates
    if ((m_curl_status = curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 0)) != CURLE_OK)
        return;
}

void UCDPCurlPoster::setVerboseOutput()
{
    if ((m_curl_status = curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1L)) != CURLE_OK)
        return;
}

void UCDPCurlPoster::setQuietOutput()
{
    if ((m_curl_status = curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 0L)) != CURLE_OK)
        return;
}

void UCDPCurlPoster::init()
{
    CURLcode res;

    /* In windows, this will init the winsock stuff */
    res = curl_global_init(CURL_GLOBAL_DEFAULT);
    /* Check for errors */
    if (res != CURLE_OK)
    {
        fprintf(stderr, "curl_global_init() failed: %s\n", curl_easy_strerror(res));
        return;
    }

    /* get a curl handle */
    m_curl = curl_easy_init();
    if (m_curl)
    {
        /* First set the URL that is about to receive our POST. */
        if ((m_curl_status = curl_easy_setopt(m_curl, CURLOPT_URL, m_url.c_str())) != CURLE_OK)
            return;

        /* Now specify we want to POST data */
        if ((m_curl_status = curl_easy_setopt(m_curl, CURLOPT_POST, 1L)) != CURLE_OK)
            return;

        if ((m_curl_status = curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 0L)) != CURLE_OK) //0 disable messages
            return;

        /*
         If you use POST to a HTTP 1.1 server, you can send data without knowing
         the size before starting the POST if you use chunked encoding. You
         enable this by adding a header like "Transfer-Encoding: chunked" with
         CURLOPT_HTTPHEADER. With HTTP 1.0 or without chunked transfer, you must
         specify the size in the request.
         */
        if (m_opts)
        {
            curl_slist_free_all(m_opts);
            m_opts = NULL;
        }
        m_opts = curl_slist_append(m_opts, "Transfer-Encoding: chunked");
        m_opts = curl_slist_append(m_opts, "Expect:");
        m_opts = curl_slist_append(m_opts, "Content-Type: application/json");
        m_curl_status = curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_opts);
        /* use curl_slist_free_all() after the *perform() call to free this
         list again */
    }
}

UCDPCurlPoster::~UCDPCurlPoster()
{
    if (m_opts)
        curl_slist_free_all(m_opts);

    if (m_resolve)
        curl_slist_free_all(m_resolve);

    if (m_curl)
        curl_easy_cleanup(m_curl);

    curl_global_cleanup();
}

size_t UCDPCurlPoster::read_callback(void *dest, size_t size, size_t nmemb, void *userp)
{
    struct WriteThis *wt = (struct WriteThis *) userp;
    size_t buffer_size = size * nmemb;

    if (wt->sizeleft)
    {
        /* copy as much as possible from the source to the destination */
        size_t copy_this_much = wt->sizeleft;
        if (copy_this_much > buffer_size)
            copy_this_much = buffer_size;
        memcpy(dest, wt->readptr, copy_this_much);

        wt->readptr += copy_this_much;
        wt->sizeleft -= copy_this_much;
        return copy_this_much; /* we copied this many bytes */
    }

    return 0; /* no more data left to deliver */
}

size_t UCDPCurlPoster::write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
    UCDPCurlPoster *thisObj = (UCDPCurlPoster *) userp;

    thisObj->m_result.assign((char *) buffer, size * nmemb);
    return size * nmemb;
}

void UCDPCurlPoster::post(std::string jstr)
{
    postIt(m_url.c_str(), jstr.c_str(), jstr.length());
}

int UCDPCurlPoster::postIt(const char *url, const char *data, int sz)
{
    CURL *curl;
    CURLcode res;

    struct WriteThis wt;

    wt.readptr = data;
    wt.sizeleft = sz;

    if (m_curl)
    {
        /* we want to use our own read function */
        curl_easy_setopt(m_curl, CURLOPT_READFUNCTION, read_callback);

        /* pointer to pass to our read function */
        curl_easy_setopt(m_curl, CURLOPT_READDATA, &wt);

        /* capture the result in our own write function */
        curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, write_data);

        /* pointer to pass to the write function */
        curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this);
        /* Perform the request, res will get the return code */
        res = curl_easy_perform(m_curl);
        /* Check for errors */
        if (res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

    }

    return 0;
}

} /* namespace S3Downloader */
