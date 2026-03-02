import ShortenForm from "./components/ShortenForm";
import "./App.css";

const App = () => {
    return (
        <div className="app-container">
            <div className="app-card">
                <div className="app-header">
                    <h1 className="app-title">URL-SHORTENER</h1>
                    <p className="app-subtitle">Shorten your URLs in seconds</p>
                </div>
                <ShortenForm />
            </div>
        </div>
    )
}

export default App;

