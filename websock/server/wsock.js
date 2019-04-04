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
     // You should not use autoAcceptConnections for production
     // applications, as it defeats all standard cross-origin protection
     // facilities built into the protocol and the browser.  You should
     // *always* verify the connection's origin and decide whether or not
     // to accept it.
     autoAcceptConnections: false
   }                          );
 
wsServer.on('request', function(request)
   {
    var connection = request.accept('apl-protocol', request.origin);
    var tx_line = "";
    console.log(now() + ' Connection from ' +
                request.origin + ' accepted.');

     apl = spawn('/usr/local/bin/apl', [ "--safe",
                                         "--noSV",
                                         "--noCONT",
                                         "--rawCIN",
                                         "-p", "2",
                                         "-w", "100",
                                       ]);
     apl.stdout.setEncoding('utf-8');

     apl.stdout.on('data', (data) =>
        {
          // some output bytes from GNU APL.
          // Collect them into full lines.
          var pos = data.lastIndexOf("\n");
	  if (pos == -1)   tx_line += data;
          else
             {
               var rest = data.slice(pos + 1)   // bytes after \n
               connection.sendUTF(tx_line + data.slice(0, pos + 1));
               tx_line = rest;
             }
        }         );

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

