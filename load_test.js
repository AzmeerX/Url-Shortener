import http from "k6/http";
import { sleep } from "k6";

export let options = {
    vus: 100, // Number of virtual users
    duration: "15s", // Duration of the test
}

export default function () {
    // 10 read
    for(let i = 0; i < 10; i++) {
        http.get("http://localhost:5555/aH5uYO");
    }

    // 1 write
    http.post("http://localhost:5555/shorten", JSON.stringify({
        long_url: `https://example.com/${Math.random()}`
    }), {
        headers: { "Content-Type": "application/json" }
    });

    sleep(0.1); // Sleep for 0.1s between iterations
}