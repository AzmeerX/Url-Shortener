import { useEffect, useMemo, useState } from "react";
import { getAnalytics } from "../api";
import "./AnalyticsPanel.css";

function formatDate(value) {
    if (!value) return "Never";
    const parsed = new Date(value.replace(" ", "T"));
    if (Number.isNaN(parsed.getTime())) return value;
    return parsed.toLocaleString();
}

function getShortCodeFromUrl(url) {
    if (!url) return "";
    try {
        const parsed = new URL(url);
        return parsed.pathname.replace(/^\//, "").trim();
    } catch {
        return "";
    }
}

export default function AnalyticsPanel({ shortUrl }) {
    const initialCode = useMemo(() => getShortCodeFromUrl(shortUrl), [shortUrl]);
    const [inputCode, setInputCode] = useState(initialCode);
    const [analytics, setAnalytics] = useState(null);
    const [loading, setLoading] = useState(false);
    const [error, setError] = useState("");

    useEffect(() => {
        setInputCode(initialCode);
        if (initialCode) {
            fetchAnalytics(initialCode);
        } else {
            setAnalytics(null);
            setError("");
        }
        // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [initialCode]);

    const fetchAnalytics = async (code) => {
        const normalizedCode = (code || "").trim();
        if (!normalizedCode) {
            setError("Enter a short code to fetch analytics");
            setAnalytics(null);
            return;
        }

        setLoading(true);
        setError("");
        try {
            const data = await getAnalytics(normalizedCode);
            setAnalytics(data);
        } catch (err) {
            setAnalytics(null);
            setError(err.message || "Failed to fetch analytics");
        } finally {
            setLoading(false);
        }
    };

    const isExpired = analytics?.expiresAt
        ? new Date(analytics.expiresAt.replace(" ", "T")).getTime() < Date.now()
        : false;

    return (
        <section className="analytics-panel" aria-live="polite">
            <div className="analytics-header-row">
                <div>
                    <h2 className="analytics-title">Link Analytics</h2>
                    <p className="analytics-subtitle">Track clicks, creation time, and expiration status.</p>
                </div>
                <button
                    type="button"
                    className="analytics-refresh"
                    onClick={() => fetchAnalytics(inputCode)}
                    disabled={loading}
                >
                    {loading ? "Refreshing..." : "Refresh"}
                </button>
            </div>

            <div className="analytics-search-row">
                <input
                    type="text"
                    value={inputCode}
                    onChange={(e) => setInputCode(e.target.value)}
                    placeholder="Enter short code (e.g. Ab12Cd)"
                    className="analytics-input"
                    disabled={loading}
                />
                <button
                    type="button"
                    className="analytics-button"
                    onClick={() => fetchAnalytics(inputCode)}
                    disabled={loading}
                >
                    {loading ? "Loading..." : "Load Analytics"}
                </button>
            </div>

            {error && <p className="analytics-error">{error}</p>}

            {analytics && !error && (
                <div className="analytics-grid">
                    <article className="analytics-stat card-accent-cyan">
                        <p className="stat-label">Clicks</p>
                        <p className="stat-value">{analytics.clickCount ?? 0}</p>
                    </article>
                    <article className="analytics-stat card-accent-amber">
                        <p className="stat-label">Status</p>
                        <p className="stat-value stat-status">{isExpired ? "Expired" : "Active"}</p>
                    </article>
                    <article className="analytics-stat">
                        <p className="stat-label">Short Code</p>
                        <p className="stat-value">{analytics.shortCode}</p>
                    </article>
                    <article className="analytics-stat">
                        <p className="stat-label">Original URL</p>
                        <a
                            href={analytics.originalUrl}
                            target="_blank"
                            rel="noopener noreferrer"
                            className="stat-link"
                        >
                            {analytics.originalUrl}
                        </a>
                    </article>
                    <article className="analytics-stat">
                        <p className="stat-label">Created At</p>
                        <p className="stat-meta">{formatDate(analytics.createdAt)}</p>
                    </article>
                    <article className="analytics-stat">
                        <p className="stat-label">Expires At</p>
                        <p className="stat-meta">{formatDate(analytics.expiresAt)}</p>
                    </article>
                </div>
            )}
        </section>
    );
}
