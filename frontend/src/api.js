const BASE_URL = "http://localhost:5555";

export const shortenUrl = async (longUrl) => {
    const res = await fetch(`${BASE_URL}/shorten`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ long_url: longUrl }),
    });

    const contentType = res.headers.get("content-type") || "";

    if (!res.ok) {
        let message = "Failed to shorten URL";
        if (contentType.includes("application/json")) {
            const data = await res.json();
            message = data?.message || data?.error || message;
        } else {
            const text = await res.text();
            if (text?.trim()) {
                message = text.trim();
            }
        }
        throw new Error(message);
    }

    return res.json();
};

export const getAnalytics = async (shortCode) => {
    const normalizedCode = (shortCode || "").trim();
    const res = await fetch(`${BASE_URL}/analytics/${encodeURIComponent(normalizedCode)}`);
    const contentType = res.headers.get("content-type") || "";

    if (!res.ok) {
        let message = "Failed to fetch analytics";
        if (contentType.includes("application/json")) {
            const data = await res.json();
            message = data?.message || data?.error || message;
        } else {
            const text = await res.text();
            if (text?.trim()) {
                message = text.trim();
            }
        }
        throw new Error(message);
    }

    return res.json();
};
