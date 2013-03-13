hurl async wrapper
==================

The class defined in `async_hurl.h` provides a basic way to make requests
to an HTTP server using callbacks to get the responses.


Usage
-----


#### Create client handle

Create an instance of a hurl client, managed by `boost::shared_ptr`:

```
using hurl::client;

boost::shared_ptr<client> handle;
handle = boost::make_shared<hurl::client>("http://example.com");
```

#### Wrap using async_client

Wrap it in an instance of `async_client`:

```
using hurl::async_client;

async_client client(handle);
```

#### Synchronize regularly

Synchronize the `async_client` by regularly calling its `sync` method from
one thread:

```
// Running in some thread that has access to the async_client instance
while (true) {
    client.sync();
    sleep(100);
}
```

In a real usage scenario you probably wouldn't just run `sync` continuously
in a loop. More likely, you'll call it every frame in some update function.

#### Make requests

Now you can make GET and POST requests using the `async_client` instance.
When calling `async_client::get` or `async_client::post`, the last argument
must always be a callback function that takes a single parameter of type
`hurl::httpresponse const&`. More specifically, the callback argument must
be convertible to `boost::function<void(hurl::httpresponse const&)>`.

#### Example

```
void mycallback(hurl::httpresponse const& response)
{
    std::cout << "The server says: " << response.body << "\n";
}

client.get("/hello.html", &mycallback);
```

Assuming that you have a thread periodically calling `client.sync()`, then
that eventually `mycallback` will be called in that thread and print the
server's response.

#### Other callback methods

Because the callbacks are implemented using plain function objects, you have
a lot of options for running your code to handle server responses.

For example, you can use `boost::bind` to have an object's member function
respond:

```
class Foo
{
public:
    void makerequest();
    void handleresponse(hurl::httpresponse const&);

private:
    hurl::async_client client_;
};

void Foo::makerequest()
{
    client_.get("/baz",
        boost::bind(&Foo::handleresponse, this, _1));
}

void Foo::handleresponse(hurl::httpresponse const&)
{
    // ...
}
```

If you're using a C++11-compliant compiler (and, really, you should be) an
even better option (sometimes) is to use a lambda expression to produce a
closure.

Suppose for example you want your response handler to do something with
a parameter that was passed as part of the original request. To use a free
function or a member function as your callback, you'd have to find some place
to store that argument, needlessly complicating your code. With lambdas it's
easy:

```
void get_user_fleem(std::string const& id)
{
    client.get("/fleem/" + id, [=](hurl::httpresponse const& response)
    {
        // parse response to get fleem...
        std::string fleem = parse_response_for_fleem(response);

        // print the result
        std::cout << "User " << id << "'s fleem is: " << fleem << "\n";
    });
}
```

The `id` parameter is captured in the lambda's closure, so there's no need
to find a place to store it, which would be messy and non-thread-safe.


### Object lifetimes

Because the operations are asynchronous and your callbacks are invoked
"whenever", you have to take care that anything your callbacks do will still
be safe to do at the time they are called.

For example, if your callback modifies a widget to reflect the response from
the server, you need to be sure that the widget still exists before doing
anything to it.

Sometimes checking for safety is difficult. In those cases you can take
advantage of this useful fact: when a `hurl::async_client` instance is
destroyed, all of its outstanding callbacks will be canceled. (The threads
running the requests will still run to completion, but they will not attempt
to invoke callbacks associated with  destroyed client.)

Thus, one pattern you might employ is a widget that has its own `async_client`
as a member variable; the widget can freely make requests on the client using
callbacks that will operate on itself with complete safety: if the widget is
ever destroyed, its client will be destroyed as well, canceling the callbacks.

