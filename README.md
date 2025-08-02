# Hawk-Eye
### Real-Time Market Surveillance System

##### A high-performance, real-time fraud detection and market anomaly surveillance system. Designed to ingest trading data, detect market manipulation patterns (e.g., spoofing, quote stuffing), and raise alerts—augmented with GenAI-powered summarization of suspicious activity.

---

## 📌 Features

- 📈 **Ingestion Engine**: Consumes high-volume, real-time trading data (simulated stock feeds) via **Kafka**.
- 🧩 **Detection Engine**: Rule-based detection (e.g., spoofing, price manipulation, quote stuffing) implemented in **C++**.
- 🚨 **Alert System**: Publishes alerts and insights to downstream systems via **Redis**.
- 🤖 **AI Summarization**: Uses **LangChain + OpenAI** to generate human-readable reports of flagged activity.
- 📊 **Analytics Module**: Generates real-time metrics and pattern reports for monitoring and historical analysis.
- ⚙️ **Modular Architecture**: Built with a hybrid stack using **C++**, **Python**, **librdkafka**, **Makefile**, etc.


