//
//  async_hurl.h
//
// This is a wrapper around hurl that provides asynchronous communication via
// threading and callbacks. Callbacks are managed with shared_ptr.
//
// This isn't packaged into hurl directly right now because it depends on boost,
// as well as, at the moment, the _beginthread macro defined in SexyAppFramework.
//
#pragma once

#include "hurl.h"

#include <string>
#include <set>
#include <list>

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

namespace hurl
{
    typedef boost::function<void (hurl::httpresponse const&)> callback;
    
    class async_client
    {
    public:
        explicit async_client(boost::shared_ptr<hurl::client> client);
        
        std::string cookie() const;
        void setcookie(std::string const&);
        
        //
        // response synchronization
        //
        // Requests run in separate threads, but it's often unhelpful for clients
        // to have their callbacks run in arbitrary threads. So we hold callbacks
        // in an internally synchronized queue which can be flushed regularly from
        // one main thread.
        //
        // So:
        //  1. Call sync() every frame (or at a suitable interval) from one thread.
        //  2. Issue get and post requests whenever you like.
        //  3. Your callbacks will always be run during the sync() immediately
        //     following the completion of their requests!
        //
        // Currently this class has no support for unsynchronized callbacks. You
        // *must* call sync() regularly, or your callbacks will never run!
        //
        void sync();
        void schedule(boost::function<void()> const&);
        
        
        //
        // get/post
        //
        // Perform an HTTP GET or POST request asynchronously and pass the server's
        // response to a callback when it completes. Status code and body of the
        // response are set as expected, with the additional case that if an error
        // or exception occurs while making the request such that no response can
        // possibly be received, the response code will be set to 0 and the response
        // body will contain a description of the error.
        //
        // Such cases can include a client-configured timeout or a failure to resolve
        // the destination host.
        //
        void get(std::string const& path, callback);
        void get(std::string const& path, hurl::httpparams const& params, callback);
        void post(std::string const& path, std::string const& data, callback);
        void post(std::string const& path, hurl::httpparams const& params, callback);
        
    private:
        boost::shared_ptr<hurl::client>         client_;
        std::list<boost::function<void()> >     callqueue_;
        
        // Callbacks are managed by shared pointers, so if this client
        // is destroyed while some requests are outstanding, the still-
        // running request threads won't attempt to call their callbacks.
        std::set<boost::shared_ptr<callback> >  callbacks_;
        
        boost::shared_ptr<hurl::client> copyhandle() const;
    };
}

