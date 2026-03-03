import { useState } from "react";
import { loginUser, registerUser } from "../api";
import "./AuthCard.css";

export default function AuthCard({ onAuthSuccess }) {
    const [mode, setMode] = useState("login");
    const [email, setEmail] = useState("");
    const [password, setPassword] = useState("");
    const [loading, setLoading] = useState(false);
    const [error, setError] = useState("");

    const handleSubmit = async (e) => {
        e.preventDefault();
        if (!email.trim() || !password.trim()) {
            setError("Email and password are required");
            return;
        }

        setLoading(true);
        setError("");
        try {
            const data = mode === "login"
                ? await loginUser(email.trim(), password)
                : await registerUser(email.trim(), password);
            onAuthSuccess(data);
        } catch (err) {
            setError(err.message || "Authentication failed");
        } finally {
            setLoading(false);
        }
    };

    return (
        <div className="auth-wrap">
            <div className="auth-tabs" role="tablist" aria-label="Authentication tabs">
                <button
                    className={`auth-tab ${mode === "login" ? "active" : ""}`}
                    onClick={() => setMode("login")}
                    type="button"
                >
                    Login
                </button>
                <button
                    className={`auth-tab ${mode === "register" ? "active" : ""}`}
                    onClick={() => setMode("register")}
                    type="button"
                >
                    Register
                </button>
            </div>

            <form className="auth-form" onSubmit={handleSubmit}>
                <p className="auth-caption">
                    {mode === "login"
                        ? "Sign in to create and manage your links"
                        : "Create an account to start shortening links"}
                </p>

                <input
                    className="auth-input"
                    type="email"
                    placeholder="you@example.com"
                    value={email}
                    onChange={(e) => setEmail(e.target.value)}
                    disabled={loading}
                    autoComplete="email"
                />

                <input
                    className="auth-input"
                    type="password"
                    placeholder="Password"
                    value={password}
                    onChange={(e) => setPassword(e.target.value)}
                    disabled={loading}
                    autoComplete={mode === "login" ? "current-password" : "new-password"}
                />

                <button className="auth-submit" type="submit" disabled={loading}>
                    {loading ? "Please wait..." : mode === "login" ? "Login" : "Create Account"}
                </button>

                {error && <p className="auth-error">{error}</p>}
            </form>
        </div>
    );
}
