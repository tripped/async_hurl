//
//  async_hurl.cpp
//

#include "async_hurl.h"
#ifdef WIN32
#include <process.h>
#endif
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/make_shared.hpp>

#include "SexyAppFramework/Common.h"
#include "SexyAppFramework/commonlibs.h"

namespace hurl
{
    struct thread_params
    {
        async_client* client;
        boost::shared_ptr<hurl::client> handle;
        boost::function<httpresponse()> func;
        boost::weak_ptr<callback>   cb;
    };
    
    typedef void(*threadproc)(void*);
    
    void async_request_thread(thread_params* p)
    {
        // Ensure that the thread params are freed at thread exit
        boost::scoped_ptr<thread_params> free(p);
        
        hurl::httpresponse response;
        response.status = 0;
        
        try {
            response = p->func();
        } catch (hurl::timeout const& e) {
            response.body = "timeout";
        } catch (hurl::resolve_error const& e) {
            response.body = "resolve_error";
        } catch (hurl::connect_error const& e) {
            response.body = "connect_error";
        } catch(std::runtime_error const& e) {
            response.body = e.what();
        }
        
        // Ensure that the callback hasn't been unregistered before attempting to schedule it.
        // This results in a kind of "silent error," but once we've got to this point, the
        // best we can do is avoid undefined-behavior-land.
        boost::shared_ptr<callback> cb = p->cb.lock();
        if (cb)
        {
            p->client->schedule(boost::bind(*cb, response));
            
            // Indulge in promiscuous cookie-sharing; if the handle set in
            // the thread_params has a cookie, share it with the main client
            if (!p->handle->cookie().empty())
                p->client->setcookie(p->handle->cookie());
        }
    }
    
    async_client::async_client(boost::shared_ptr<client> client)
        : client_(client)
    {
    }
    
    std::string async_client::cookie() const
    {
        return client_->cookie();
    }
    
    void async_client::setcookie(std::string const& value)
    {
        client_->setcookie(value);
    }
    
    void async_client::sync()
    {
        // TODO:
        // Currently the comment in async_hurl.h is a filthy lie.
        // This queue is not internally synchronized. This is a
        // data race, but in practice it's very unlikely to ever
        // cause a problem. Still, fix it soon.
        
        if (!callqueue_.empty())
        {
            while (!callqueue_.empty())
            {
                callqueue_.front()();
                callqueue_.pop_front();
            }
        }
    }
    
    void async_client::schedule(boost::function<void()> const& f)
    {
        callqueue_.push_back(f);
    }
    
    boost::shared_ptr<hurl::client> async_client::copyhandle() const
    {
        boost::shared_ptr<hurl::client> copy(new hurl::client(client_->base()));
        copy->setcookie(client_->cookie());
        return copy;
    }
    
    // There are a few different signatures for get/post that we use; we
    // alias them here to avoid multiline static_casts in async_client functions
    static auto get_simple = static_cast<
        httpresponse(client::*)(std::string const&)>(&client::get);
    static auto get_params = static_cast<
        httpresponse(client::*)(std::string const&, httpparams const&)>(&client::get);
    static auto post_simple = static_cast<
        httpresponse(client::*)(std::string const&, std::string const&)>(&client::post);
    static auto post_params = static_cast<
        httpresponse(client::*)(std::string const&, httpparams const&)>(&client::post);

    void async_client::get(std::string const& path, callback cb)
    {
        thread_params* p = new thread_params;
        //
        // To make this work with rudimentary pthreads, we create a copy
        // of the callback object that is managed by a shared_ptr, which
        // we store in *this; then we give the thread a weak_ptr.
        //
        boost::shared_ptr<callback> cb_ptr(new callback(cb));
        callbacks_.insert(cb_ptr);
        
        // Now, this is a bit of a hack, but to ensure thread safety we
        // make a copy of the internal client handle for each request
        p->handle = copyhandle();
        p->client = this;
        p->cb = boost::weak_ptr<callback>(cb_ptr);
        p->func = boost::bind(get_simple, p->handle, path);
        
        std::cout << "[NETWORK]   GET " << path << "\n";
        _beginthread((threadproc)&async_request_thread, 0, (void*)p);
    }
    
    void async_client::get(std::string const& path, httpparams const& params, callback cb)
    {
        thread_params* p = new thread_params;
        auto cb_ptr = boost::make_shared<callback>(cb);
        callbacks_.insert(cb_ptr);
        p->handle = copyhandle();
        p->client = this;
        p->cb = boost::weak_ptr<callback>(cb_ptr);
        p->func = boost::bind(get_params, p->handle, path, params);
        
        std::cout << "[NETWORK]   GET " << path << "\n";
        _beginthread((threadproc)&async_request_thread, 0, (void*)p);
    }

    void async_client::post(std::string const& path, std::string const& data, callback cb)
    {
        thread_params* p = new thread_params;
        boost::shared_ptr<callback> cb_ptr(new callback(cb));
        callbacks_.insert(cb_ptr);
        p->handle = copyhandle();
        p->client = this;
        p->cb = boost::weak_ptr<callback>(cb_ptr);
        p->func = boost::bind(post_simple, p->handle, path, data);
        std::cout << "[NETWORK]   POST " << path << "\n";
        _beginthread((threadproc)&async_request_thread, 0, (void*)p);
    }
    
    void async_client::post(std::string const& path, hurl::httpparams const& params, callback cb)
    {
        thread_params* p = new thread_params;
        auto cb_ptr = boost::make_shared<callback>(cb);
        callbacks_.insert(cb_ptr);
        p->handle = copyhandle();
        p->client = this;
        p->cb = boost::weak_ptr<callback>(cb_ptr);
        p->func = boost::bind(post_params, p->handle, path, params);
        std::cout << "[NETWORK]   POST " << path << "\n";
        _beginthread((threadproc)&async_request_thread, 0, (void*)p);
    }
}
