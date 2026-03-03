import { useState } from "react";
import ShortenForm from "./components/ShortenForm";
import AuthCard from "./components/AuthCard";
import "./App.css";

const App = () => {
    const [auth, setAuth] = useState(() => ({
        token: localStorage.getItem("auth_token") || "",
        email: localStorage.getItem("auth_email") || "",
    }));

    const handleAuthSuccess = (data) => {
        localStorage.setItem("auth_token", data.token);
        localStorage.setItem("auth_email", data.email || "");
        setAuth({ token: data.token, email: data.email || "" });
    };

    const handleLogout = () => {
        localStorage.removeItem("auth_token");
        localStorage.removeItem("auth_email");
        setAuth({ token: "", email: "" });
    };

    return (
        <div className="app-container">
            <div className="app-card">
                <div className="app-header">
                    <h1 className="app-title">URL-SHORTENER</h1>
                    <p className="app-subtitle">
                        {auth.token ? "Create short links and inspect analytics." : "Login or register to continue."}
                    </p>
                </div>

                {auth.token ? (
                    <>
                        <div className="session-row">
                            <span className="session-email">{auth.email || "Authenticated user"}</span>
                            <button type="button" className="logout-button" onClick={handleLogout}>
                                Logout
                            </button>
                        </div>
                        <ShortenForm onUnauthorized={handleLogout} />
                    </>
                ) : (
                    <AuthCard onAuthSuccess={handleAuthSuccess} />
                )}
            </div>
        </div>
    );
};

export default App;
