# 本地 HTTP 端到端验证

## 方式：极简 PostHog-协议 mock 接收器

```
# 1. 启动 mock 接收器
python -u deploy/mock_posthog.py 8000

# 2. 发送 demo 数据（或 SDK HttpTransport 指向 localhost:8000）
python deploy/send_demo.py http://localhost:8000 demo-data/telemetry.jsonl
```

mock 接收器打印每条收到的事件（event / distinct_id / props），并统计 received 数。

## 已废弃：完整 PostHog 自托管 Docker Compose

`docker-compose.posthog.yml` 是精简四件套（postgres+redis+clickhouse+posthog），调试发现 latest 版强制 replicated ClickHouse+内置 Keeper，单机自托管需大量配置适配。本地验证用 mock 接收器更快更可靠。
