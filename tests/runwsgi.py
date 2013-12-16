# Run a WSGI application in a daemon thread

import bottle
import threading
import socket
import time as _time

class Server(bottle.WSGIRefServer):
    def run(self, handler): # pragma: no cover
        from wsgiref.simple_server import make_server, WSGIRequestHandler
        if self.quiet:
            base = self.options.get('handler_class', WSGIRequestHandler)
            class QuietHandler(base):
                def log_request(*args, **kw): pass
            self.options['handler_class'] = QuietHandler
        self.srv = make_server(self.host, self.port, handler, **self.options)
        self.srv.serve_forever(poll_interval=0.1)

def wait_until_started(netloc):
    print 'starting'
    ok = False
    for i in range(10):
        try:
            conn = socket.create_connection(netloc, 0.1)
        except socket.error as e:
            _time.sleep(0.1)
        else:
            conn.close()
            ok = True
            break
    return ok

def wait_until_stopped(netloc):
    print 'stopping'
    ok = False
    for i in range(10):
        try:
            conn = socket.create_connection(netloc, 0.1)
        except socket.error as e:
            ok = True
            break
        else:
            conn.close()
    _time.sleep(1)
    return ok

def start_bottle_server(app, port, **kwargs):
    server_thread = ServerThread(app, port, kwargs)
    server_thread.daemon = True
    server_thread.start()
    
    ok = wait_until_started(('127.0.0.1', port))
    if not ok:
        import warnings
        warnings.warn('Server did not start after 1 second')
    
    return server_thread.server

class ServerThread(threading.Thread):
    def __init__(self, app, port, server_kwargs):
        threading.Thread.__init__(self)
        self.app = app
        self.port = port
        self.server_kwargs = server_kwargs
        self.server = Server(host='localhost', port=self.port, **self.server_kwargs)
    
    def run(self):
        bottle.run(self.app, server=self.server, quiet=True)

def app_runner_setup(*specs):
    '''Returns setup and teardown methods for running a list of WSGI
    applications in a daemon thread.
    
    Each argument is an (app, port) pair.
    
    Return value is a (setup, teardown) function pair.
    
    The setup and teardown functions expect to be called with an argument
    on which server state will be stored.
    
    Example usage with nose:
    
    >>> setup_module, teardown_module = \
        runwsgi.app_runner_setup((app_module.app, 8050))
    '''
    
    def setup(self):
        self.servers = []
        for spec in specs:
            if len(spec) == 2:
                app, port = spec
                kwargs = {}
            else:
                app, port, kwargs = spec
            server = start_bottle_server(app, port, **kwargs)
            server.__runwsgi_port = port
            self.servers.append(server)
    
    def teardown(self):
        for server in self.servers:
            # if no tests from module were run, there is no server to shut down
            if hasattr(server, 'srv'):
                server.srv.shutdown()
                # should not be necessary, but just in case
                ok = wait_until_stopped(('localhost', server.port))
                if not ok:
                    import warnings
                    warnings.warn('Server did not stop after 1 second')
                print ok
    
    return [setup, teardown]
