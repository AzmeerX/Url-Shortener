const BASE_URL = "http://localhost";

const getStoredToken = () => localStorage.getItem("auth_token");

const buildHeaders = (needsAuth = false) => {
    const headers = { "Content-Type": "application/json" };
    if (needsAuth) {
        const token = getStoredToken();
        if (token) {
            headers.Authorization = `Bearer ${token}`;
        }
    }
    return headers;
};

const readError = async (res, fallbackMessage) => {
    const contentType = res.headers.get("content-type") || "";
    let message = fallbackMessage;
    if (contentType.includes("application/json")) {
        const data = await res.json();
        message = data?.message || data?.error || message;
    } else {
        const text = await res.text();
        if (text?.trim()) {
            message = text.trim();
        }
    }
    const error = new Error(message);
    error.status = res.status;
    throw error;
};

export const shortenUrl = async (longUrl) => {
    const res = await fetch(`${BASE_URL}/shorten`, {
        method: "POST",
        headers: buildHeaders(true),
        body: JSON.stringify({ long_url: longUrl }),
    });

    if (!res.ok) {
        return readError(res, "Failed to shorten URL");
    }

    return res.json();
};

export const getAnalytics = async (shortCode) => {
    const normalizedCode = (shortCode || "").trim();
    const res = await fetch(`${BASE_URL}/analytics/${encodeURIComponent(normalizedCode)}`);

    if (!res.ok) {
        return readError(res, "Failed to fetch analytics");
    }

    return res.json();
};

export const registerUser = async (email, password) => {
    const res = await fetch(`${BASE_URL}/register`, {
        method: "POST",
        headers: buildHeaders(false),
        body: JSON.stringify({ email, password }),
    });

    if (!res.ok) {
        return readError(res, "Failed to register");
    }

    return res.json();
};

export const loginUser = async (email, password) => {
    const res = await fetch(`${BASE_URL}/login`, {
        method: "POST",
        headers: buildHeaders(false),
        body: JSON.stringify({ email, password }),
    });

    if (!res.ok) {
        return readError(res, "Failed to login");
    }

    return res.json();
};
