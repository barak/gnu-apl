#!/usr/bin/env node

var WebSocketServer = require('websocket').server;
var http = require('http');
const { spawn } = require('child_process');

function now()
   {
     var d = new Date();
     var hh = `00${d.getHours()}`  .slice(-2)
     var mm = `00${d.getMinutes()}`.slice(-2)
     var ss = `00${d.getSeconds()}`.slice(-2)
     return `${hh}:${mm}:${ss}`
   }

var server = http.createServer(
   function(request, response)
      {
        console.log(now() + ' Received request for ' + request.url);
        response.writeHead(404);
        response.end();
      }                       );

server.listen(42424,
   function()
      {
        console.log(now() + ' Server is listening on port 42424');
      }      );

wsServer = new WebSocketServer(
   {
     httpServer: server,
     autoAcceptConnections: false
   }                          );
 
wsServer.on('request', function(request)
   {
    var connection = request.accept('apl-protocol', request.origin);
    console.log(now() + ' Connection from ' +
                request.origin + ' accepted.');

     apl = spawn('/usr/local/bin/apl', [ "--safe",
                                         "--noSV",
                                         "--noCONT",
                                         "--rawCIN",
                                         "-p", "2",
                                         "-w", "300",
                                       ]);
     apl.stdout.setEncoding('utf-8');
     apl.stdout.on('data', (data) => { connection.sendUTF(data); } );
     apl.stderr.setEncoding('utf-8');
     apl.stderr.on('data', (data) => { connection.sendUTF(data); } );

    connection.on('message',
       function(message)
          {
            // the user has entered a line in her browser.
            // Forward the line to GNU APL.
            //
            if (message.type === 'utf8')
               {
                 apl.stdin.write(message.utf8Data);
               }
          }    );

    connection.on('close',
       function(reasonCode, description)
          {
            // The browser has closed the connection.
            //
            console.log(now() + ' Peer ' + connection.remoteAddress +
                        ' disconnected.');
            apl.stdin.write(')OFF');
            apl.kill('SIGHUP');
          }      );

    connection.on('error',
       function(reasonCode, description)
          {
            console.log(now() + ' Error on connection: ' + description
                        );
          }      );

   }       );

