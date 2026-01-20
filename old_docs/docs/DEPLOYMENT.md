# HeimWatt Deployment Guide

## For Users: Installing HeimWatt

### Prerequisites
- Docker and Docker Compose installed
- Linux (x86_64 or arm64)

### Quick Start

```bash
# Download docker-compose.yml
curl -sSL https://heimwatt.io/docker-compose.yml -o docker-compose.yml

# Start HeimWatt
docker compose up -d
```

HeimWatt is now running. Access via:
- `http://localhost:8080` (same machine)
- `http://heimwatt.local` (from any device on LAN, requires mDNS/Avahi)


### Directory Structure

```
./
├── docker-compose.yml
├── config/          # Your configuration
│   └── config.yaml
├── data/            # Database (auto-created)
└── plugins/         # Custom plugins (optional)
```

### Automatic Updates

The docker-compose.yml includes [Watchtower](https://containrrr.dev/watchtower/), which automatically:
1. Checks for new images every 24 hours
2. Downloads new versions
3. Restarts containers seamlessly

**No manual updates required.**

### Manual Update (if needed)

```bash
docker compose pull
docker compose up -d
```

### Rollback

```bash
# List available versions
docker images heimwatt/heimwatt

# Use specific version
docker compose down
# Edit docker-compose.yml: image: heimwatt/heimwatt:v1.2.3
docker compose up -d
```

---

## For Developers: Building & Releasing

### Container Registry

Images are published to GitHub Container Registry:
```
ghcr.io/heimwatt/heimwatt:latest
ghcr.io/heimwatt/heimwatt:v1.2.3
```

### CI/CD Pipeline

On every push to `main`:
1. Run tests
2. Build multi-arch image (amd64 + arm64)
3. Push to ghcr.io with `:latest` tag

On every tag (`v*`):
1. All the above
2. Also push with version tag (`:v1.2.3`)

### Dockerfile

```dockerfile
FROM alpine:3.19 AS builder
RUN apk add --no-cache gcc musl-dev make sqlite-dev curl-dev
WORKDIR /build
COPY . .
RUN make release

FROM alpine:3.19
RUN apk add --no-cache sqlite-libs libcurl
COPY --from=builder /build/heimwatt /usr/local/bin/
COPY --from=builder /build/plugins /usr/local/lib/heimwatt/plugins
EXPOSE 8080
VOLUME ["/data", "/config"]
ENTRYPOINT ["heimwatt"]
CMD ["--config", "/config/config.yaml"]
```

### docker-compose.yml (distributed to users)

```yaml
version: "3.8"
services:
  heimwatt:
    image: ghcr.io/heimwatt/heimwatt:latest
    container_name: heimwatt
    restart: unless-stopped
    volumes:
      - ./data:/data
      - ./config:/config
      - ./plugins:/plugins
    ports:
      - "8080:8080"

  watchtower:
    image: containrrr/watchtower
    container_name: watchtower
    restart: unless-stopped
    volumes:
      - /var/run/docker.sock:/var/run/docker.sock
    command: --interval 86400 --cleanup heimwatt
```

### GitHub Actions Workflow

```yaml
# .github/workflows/release.yml
name: Release

on:
  push:
    branches: [main]
    tags: ['v*']

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Set up QEMU
        uses: docker/setup-qemu-action@v3
      
      - name: Set up Docker Buildx
        uses: docker/setup-buildx-action@v3
      
      - name: Login to GHCR
        uses: docker/login-action@v3
        with:
          registry: ghcr.io
          username: ${{ github.actor }}
          password: ${{ secrets.GITHUB_TOKEN }}
      
      - name: Build and push
        uses: docker/build-push-action@v5
        with:
          push: true
          platforms: linux/amd64,linux/arm64
          tags: |
            ghcr.io/heimwatt/heimwatt:latest
            ghcr.io/heimwatt/heimwatt:${{ github.ref_name }}
```

### Testing Release Locally

```bash
# Build local image
docker build -t heimwatt:test .

# Test with local docker-compose
docker compose -f docker-compose.dev.yml up
```
