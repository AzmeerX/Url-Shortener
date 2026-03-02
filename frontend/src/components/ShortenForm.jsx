import { useState } from "react";
import { shortenUrl } from "../api";
import "./ShortenForm.css";

export default function ShortenForm() {
    const [longUrl, setLongUrl] = useState("");
    const [shortUrl, setShortUrl] = useState("");
    const [loading, setLoading] = useState(false);
    const [error, setError] = useState("");
    const [copied, setCopied] = useState(false);

    const handleSubmit = async (e) => {
        e.preventDefault();
        if (!longUrl.trim()) {
            setError("Please enter a URL");
            return;
        }

        setLoading(true);
        setError("");
        try {
            const data = await shortenUrl(longUrl);
            setShortUrl(data.shortUrl);
        } catch (err) {
            setError(err.message || "Failed to shorten URL. Please try again.");
        } finally {
            setLoading(false);
        }
    }

    const handleCopy = () => {
        navigator.clipboard.writeText(shortUrl);
        setCopied(true);
        setTimeout(() => setCopied(false), 2000);
    }

    return (
        <div className="form-container">
            <form onSubmit={handleSubmit} className="shorten-form">
                <div className="input-group">
                    <input
                        type="url"
                        placeholder="Paste your long URL here..."
                        value={longUrl}
                        onChange={(e) => setLongUrl(e.target.value)}
                        className="url-input"
                        disabled={loading}
                    />
                    <button 
                        type="submit" 
                        className="submit-button"
                        disabled={loading}
                    >
                        {loading ? "Shortening..." : "Shorten"}
                    </button>
                </div>
                {error && <p className="error-message">{error}</p>}
            </form>

            {shortUrl && (
                <div className="result-container">
                    <p className="result-label">Your shortened URL:</p>
                    <div className="result-box">
                        Short URL: <a href={shortUrl} target="_blank" rel="noopener noreferrer">{shortUrl}</a>
                        <button 
                            onClick={handleCopy}
                            className={`copy-button ${copied ? 'copied' : ''}`}
                        >
                            {copied ? '✓ Copied' : 'Copy'}
                        </button>
                    </div>
                </div>
            )}
        </div>
    );
}
