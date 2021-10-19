const http = require('http');
const https = require('https')
const express = require('express');
const app = express();
const server = http.createServer(app);
const fs = require('fs');
const url = require('url');
const nocache = require('nocache');
const cors = require('cors');
const path = require('path');

let port = 8000;
if (process.argv[2]) {
    port = process.argv[2];
}

let skipssl = 0;
if (process.argv[3]) {
    skipssl = process.argv[3];
}

let certpath = "/etc/letsencrypt";
if (process.argv[4]) {
    certpath = process.argv[4];
}

app.use(cors())
app.use(nocache());

app.get('/', cors(), function(req, res) {
    if (!req.secure && skipssl != 1) {
        return res.redirect("https://" + req.headers.host + req.url);
    }
    res.sendFile(path.join(__dirname + '/index.html'));
    res.header('Cross-Origin-Embedder-Policy','require-corp');
    res.header('Cross-Origin-Opener-Policy','same-origin');
    res.header('Cross-Origin-Resource-Policy','same-origin');
});

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

if (skipssl != 1){
    https.createServer({
            key: fs.readFileSync(`${certpath}/privkey.pem`),
            cert: fs.readFileSync(`${certpath}/cert.pem`),
            ca: fs.readFileSync(`${certpath}/chain.pem`),
        },
        app
    ).listen(443, () => {
        console.log('Listening https port 443')
    })
}

const Helper = {
    types: {
       "wasm" : "application/wasm"
       , "js" : "application/javascript"
       , "html" : "text/html"
       , "htm" : "text/html"
       , "ico" : "image/vnd.microsoft.icon"
       , "css" : "text/css"
    },
    getMime: function(u) {
        var ext = this.getExt(u.pathname).replace('.', '');
        return this.types[ext.toLowerCase()] || 'application/octet-stream';
    },
    getExt: function(path) {
        var i = path.lastIndexOf('.');
        return (i < 0) ? '' : path.substr(i);
    }
};

server.listen(port);
console.log(`HTTP and BinaryJS server started on port ${port}`);