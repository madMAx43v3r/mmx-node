import express from "express";
import { createProxyMiddleware } from "http-proxy-middleware";

const host = "localhost";
const port = 3300;
const target = "http://localhost:3000";

const app = express();

const getProxyMiddleware = (basepath = "") =>
    createProxyMiddleware({
        target: `${target}${basepath}`,
        changeOrigin: true,
    });

const log = (req, res, next) => {
    console.log(req.originalUrl);
    next();
};

const error500 = (req, res, next) => {
    return res.status(500).send({
        message: "This is an error!",
    });
};

const latency = (req, res, next) => {
    setTimeout(next, 20000);
};

app.use("/server", [
    //
    //log,
    //latency,
    //error500,
    getProxyMiddleware("/server"),
]);

app.use("/api", [
    //
    //log,
    //latency,
    //error500,
    getProxyMiddleware("/api"),
]);

app.use("/wapi", [
    //
    //log,
    //latency,
    //error500,
    getProxyMiddleware("/wapi"),
]);

app.use("/", getProxyMiddleware());

// Start the server
const server = app.listen(port, host, () => {
    console.log(`Listening at http://${host}:${port}`);
});

// // Define the target WebSocket server
// const wsProxy = createProxyMiddleware({
//     target,
//     changeOrigin: true,
// });
// // Handle WebSocket upgrade
// server.on("upgrade", wsProxy.upgrade);
