require(__dirname  + '/helper.js');

const http = require('http');
const express = require('express');
const app = express();
const server = http.createServer(app);
const fs = require('fs');
const url = require('url');
const nocache = require('nocache');
const cors = require('cors');

let port = 8000;

app.use(cors())
app.use(nocache());
app.get('/*.*' , cors(), function(req, res) {
    var options = url.parse(req.url, true);
    var mime = Helper.getMime(options);
    serveFile(res, options.pathname, mime);
});

function serveFile(res, pathName, mime) {
    mime = mime || 'text/html';
    fs.readFile(__dirname + '/' + pathName, function (err, data) {
        if (err) {
            res.writeHead(500, {"Content-Type": "text/plain"});
            return res.end('Error loading ' + pathName + " with Error: " + err);
        }
        res.header('Cross-Origin-Embedder-Policy','require-corp');
        res.header('Cross-Origin-Opener-Policy','same-origin');
        res.header('Cross-Origin-Resource-Policy','same-origin');
        res.writeHead(200, {"Content-Type": mime});
        res.end(data);
    });
}

server.listen(port);
console.log(`HTTP and BinaryJS server started on port ${port}`);
