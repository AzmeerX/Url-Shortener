# URL Shortener

A production-style URL shortener with JWT auth, Redis caching, Postgres persistence, rate limiting, and a React dashboard for link analytics.

**Highlights**
- JWT-based auth for creating short links
- Redis cache with TTL for hot redirects
- Postgres-backed analytics and optional link expiration
- Per-IP rate limiting on `POST /shorten`
- Nginx reverse proxy + Docker Compose stack

**Tech Stack**
- Backend: C++ (Drogon)
- Frontend: React + Vite
- Data: Postgres, Redis
- Infra: Docker, Nginx
- Load testing: k6

**Quickstart (Docker Compose)**
This brings up the backend, database, cache, Nginx, and monitoring stack.

```bash
cd shortener
docker compose up --build
```

Default endpoints:
- App (via Nginx): `http://localhost`
- Redis: `localhost:6379`

**Frontend (Vite Dev Server)**
Run the UI separately during development.

```bash
cd frontend
npm install
npm run dev
```

**API Reference (Core)**
- `POST /register` Body: `{ "email": "user@example.com", "password": "Passw0rd!" }`
- `POST /login` Body: `{ "email": "user@example.com", "password": "Passw0rd!" }`
- `POST /shorten` Header: `Authorization: Bearer <token>` Body: `{ "long_url": "https://example.com", "ttl_seconds": 3600 }`
- `GET /{short_code}` Redirects to the original URL (302), returns 410 if expired
- `GET /analytics/{short_code}` Returns click count and timestamps

**Environment Variables (Backend)**
- `PORT` (default `5555`)
- `DB_HOST`, `DB_PORT`, `DB_NAME`, `DB_USER`, `DB_PASS`, `DB_SSLMODE`
- `REDIS_URL` or `REDIS_PUBLIC_URL` (preferred)
- `REDIS_HOST`, `REDIS_PORT`, `REDIS_USER`, `REDIS_PASSWORD`
- `JWT_SECRET` (use 16+ chars in production)
- `FRONTEND_ORIGIN` (extra CORS allowlist entry)

**Load Testing (k6)**
The script registers a user, logs in, creates one link, and then hammers reads.

```bash
k6 run load_test.js
```

**Repo Structure**
- `shortener/` Backend service, Docker Compose stack, monitoring, and DB init scripts
- `frontend/` React UI
- `load_test.js` k6 performance test
- `Dockerfile` Root container build for the backend

**Notes**
- The short link generator is deterministic; collisions are retried with a modified seed.
- `POST /shorten` is rate-limited to 60 requests per minute per IP.
