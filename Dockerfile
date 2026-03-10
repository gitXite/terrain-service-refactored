FROM node:25.2.1-slim AS builder

RUN apt-get update && apt-get install -y \
  build-essential \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY package*.json ./
RUN npm ci --only=production

COPY . .

RUN make
RUN chmod +x ./celevstl

FROM node:25.2.1-slim

WORKDIR /app

COPY --from=builder /app /app

COPY hgt_files /app/hgt_files

EXPOSE 8080

ENV NODE_ENV=production

CMD ["node", "index.js"]
