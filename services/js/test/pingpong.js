#!mojo:js_content_handler

define("main", [
  "mojo/services/public/js/application",
  "services/js/test/pingpong_service.mojom",
], function(appModule, ppModule) {

  class PingPongServiceImpl {
    constructor(app, client) {
      this.app = app;
      this.client = client;
    }

    ping(value) {
      this.client.pong(value + 1);
    };

    quit() {
      this.app.quit();
    }

    // This method is only used by the PingTargetURL test.
    pingTargetURL(url, count) {
      var app = this.app;
      return new Promise(function(resolve) {
        var pingTargetClient = {
          pong: function(value) {
            if (value == count) {
              pingTargetService.quit();
              resolve({ok: true});
            }
          }
        };
        var pingTargetService = app.shell.connectToService(
            url, ppModule.PingPongService, pingTargetClient);
        for(var i = 0; i <= count; i++)
          pingTargetService.ping(i);
      });
    }

    // This method is only used by the PingTargetService test.
    pingTargetService(handle, count) {
      var pingTargetService = new ppModule.PingPongService.proxyClass(handle);
      return new Promise(function(resolve) {
        var pingTargetClient = {
          pong: function(value) {
            if (value == count) {
              pingTargetService.quit();
              resolve({ok: true});
            }
          }
        };
        pingTargetService.client$ = pingTargetClient;
        for(var i = 0; i <= count; i++)
          pingTargetService.ping(i);
      });
    }
  }

  class PingPong extends appModule.Application {
    acceptConnection(url, serviceProvider) {
      serviceProvider.provideService(
          ppModule.PingPongService, PingPongServiceImpl.bind(null, this));
    }
  }

  return PingPong;
});
