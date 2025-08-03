#include <iostream>
#include <deque>
#include <string>
#include <map>
#include <json/json.h>
#include <librdkafka/rdkafka.h>
#include <cmath>

using namespace std;

extern void send_alert(const std::string& json_message); // from kafka_producer.cpp

// Parameters
const int PRICE_WINDOW = 50;
const double MAX_DEVIATION = 0.10; // 10%

map<string, deque<double>> recent_prices_per_symbol;

static void error_cb(rd_kafka_t* rk, int err, const char* reason, void* opaque) {
    fprintf(stderr, "Kafka error: %s\n", rd_kafka_err2str((rd_kafka_resp_err_t)err));
}

double is_price_deviating(const string& symbol, double price) {
    auto& prices = recent_prices_per_symbol[symbol];

    if (prices.size() < 10)
        return 0;

    double sum = 0.0;
    for (double p : prices)
        sum += p;
    double avg = sum / prices.size();

    return fabs(price - avg) / avg;
}

int main() {
    char errstr[512];

    // Kafka consumer config
    rd_kafka_conf_t* conf = rd_kafka_conf_new();
    rd_kafka_conf_set(conf, "bootstrap.servers", "localhost:9092", errstr, sizeof(errstr));
    rd_kafka_conf_set(conf, "group.id", "hawk-eye-price-detector-symbol-wise", errstr, sizeof(errstr));
    rd_kafka_conf_set(conf, "auto.offset.reset", "earliest", errstr, sizeof(errstr));
    rd_kafka_conf_set(conf, "enable.auto.commit", "true", errstr, sizeof(errstr));
    rd_kafka_conf_set_error_cb(conf, error_cb);

    rd_kafka_t* consumer = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr, sizeof(errstr));
    if (!consumer) {
        cerr << "Failed to create Kafka consumer: " << errstr << endl;
        return 1;
    }
    rd_kafka_poll_set_consumer(consumer);

    rd_kafka_topic_partition_list_t* topics = rd_kafka_topic_partition_list_new(1);
    rd_kafka_topic_partition_list_add(topics, "order_feed", -1);
    if (rd_kafka_subscribe(consumer, topics) != RD_KAFKA_RESP_ERR_NO_ERROR) {
        cerr << "Failed to subscribe to topic." << endl;
        return 1;
    }

    cout << "--- Symbol-wise Price Deviation Detector started ---" << endl;

    while (true) {
        rd_kafka_message_t* msg = rd_kafka_consumer_poll(consumer, 1000);
        if (!msg) continue;

        if (msg->err) {
            cerr << "Kafka message error: " << rd_kafka_message_errstr(msg) << endl;
            rd_kafka_message_destroy(msg);
            continue;
        }

        string payload((char*)msg->payload, msg->len);
        Json::Value root;
        Json::CharReaderBuilder builder;
        string errs;

        auto reader = builder.newCharReader();
        if (!reader->parse(payload.c_str(), payload.c_str() + payload.size(), &root, &errs)) {
            cerr << "JSON parse error: " << errs << "\nPayload: " << payload << endl;
            rd_kafka_message_destroy(msg);
            continue;
        }

        if (!root.isMember("price") || !root.isMember("symbol") ||
            !root["price"].isNumeric() || !root["symbol"].isString()) {
            cerr << "Missing or invalid 'price' or 'symbol' field in: " << payload << endl;
            rd_kafka_message_destroy(msg);
            continue;
        }

        string symbol = root["symbol"].asString();
        double curr_price = root["price"].asDouble();

        auto& prices = recent_prices_per_symbol[symbol];
        if (prices.size() >= PRICE_WINDOW)
            prices.pop_front();
        prices.push_back(curr_price);

        double deviation = is_price_deviating(symbol, curr_price);
        cout << "Symbol: " << symbol << ", Price: " << curr_price << ", Deviation: " << deviation << endl;

        if (deviation > MAX_DEVIATION) {
            Json::Value alert;
            alert["alert_type"] = "price_deviation";
            alert["symbol"] = symbol;
            alert["order_id"] = root["order_id"];
            alert["user_id"] = root["user_id"];
            alert["price"] = curr_price;
            alert["timestamp"] = root["timestamp"];
            alert["evidence"] = "Price for " + symbol + " deviated (" + to_string(deviation * 100) + "%) >10% from recent average";

            Json::StreamWriterBuilder writer;
            string alert_msg = Json::writeString(writer, alert);

            send_alert(alert_msg);
            cout << "Price deviation alert sent: " << alert_msg << endl;
        }

        rd_kafka_message_destroy(msg);
    }

    rd_kafka_consumer_close(consumer);
    rd_kafka_destroy(consumer);
    return 0;
}
