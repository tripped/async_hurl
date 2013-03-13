hurl async wrapper
==================

The class defined in `async_hurl.h` provides a basic way to make requests
to an HTTP server using callbacks to get the responses.


Usage
-----

Create an instance of a hurl client, managed by `boost::shared_ptr`:

```
using hurl::client;

boost::shared_ptr<client> handle;
handle = boost::make_shared<hurl::client>("http://example.com");
```

Wrap it in an instance of `async_client`:

```
using hurl::async_client;

async_client client(handle);
```

Synchronize the `async_client` by calling its `sync` method at regular
intervals from the thread that you want your callbacks to run in.

```
// Running in some thread that has access to the async_client instance
while (true) {
    client.sync();
    sleep(100);
}
```

In a real use case you probably wouldn't just run `sync` continuously in a
loop.

Now you can make GET and POST requests using the `async_client` instance.
When calling `async_client::get` or `async_cient::post`, the last argument
must always be a callback function that takes a single parameter of type
`hurl::httpresponse const&`. More specifically, the callback argument must
be convertible to `boost::function<void(hurl::httpresponse const&)>`.

Example:

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

