import http from "k6/http";
import { sleep } from "k6";

// Load profile
export let options = {
    vus: 30,
    duration: "20s",
    maxRedirects: 0, // treat 302 as success without following to external hosts
};

// Hit nginx on port 80 by default; override with BASE_URL env if needed.
const BASE = __ENV.BASE_URL || "http://localhost";
const EMAIL = __ENV.K6_EMAIL || "k6@test.local";
const PASSWORD = __ENV.K6_PASSWORD || "Passw0rd!";

function ensureOk(res, label) {
    if (!res || res.status < 200 || res.status >= 300) {
        throw new Error(`${label} failed with status ${res && res.status}`);
    }
    return res;
}

// Prepare: ensure we have a token and a known short code to hit in the hot path
export function setup() {
    const headers = { "Content-Type": "application/json" };

    // Register (ignore failure if user exists)
    http.post(
        `${BASE}/register`,
        JSON.stringify({ email: EMAIL, password: PASSWORD }),
        { headers }
    );

    // Login to get JWT
    const loginRes = http.post(
        `${BASE}/login`,
        JSON.stringify({ email: EMAIL, password: PASSWORD }),
        { headers }
    );
    ensureOk(loginRes, "login");
    const token = loginRes.json("token");

    // Create one canonical short link to hammer with reads
    const shortenRes = http.post(
        `${BASE}/shorten`,
        JSON.stringify({ long_url: "https://example.com/bootstrap" }),
        { headers: { ...headers, Authorization: `Bearer ${token}` } }
    );
    ensureOk(shortenRes, "shorten");
    const shortCode = shortenRes.json("shortCode") || shortenRes.json("short_code");

    return { token, shortCode };
}

export default function ({ token, shortCode }) {
    const headers = {
        Authorization: `Bearer ${token}`,
        "Content-Type": "application/json",
    };

    // 10 reads against an existing short code (should be 302)
    for (let i = 0; i < 10; i++) {
        http.get(`${BASE}/${shortCode}`);
    }

    // Write less frequently to avoid app's per-IP rate limit (50 req/min).
    // Every 10th iteration per VU performs a write; others skip.

    // if (__ITER % 10 === 0) {
    //     http.post(
    //         `${BASE}/shorten`,
    //         JSON.stringify({ long_url: `https://example.com/${Math.random()}` }),
    //         { headers }
    //     );
    // }

    sleep(0.1);
}
