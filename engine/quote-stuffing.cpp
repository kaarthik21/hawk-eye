#include <iostream>
#include <string>
#include <deque>
#include <map>
#include <json/json.h>
#include <librdkafka/rdkafka.h>

using namespace std;

extern void send_alert(const std::string& json_message);

struct Order {
    string order_id;
    string order_type;
    double price;
    string symbol;
    int quantity;
    long long timestamp;
    string user_id;
};

// Configurable parameters
const int WINDOW_MS = 3000;          // Time window in milliseconds
const int MAX_ORDERS_IN_WINDOW = 15; // More than this = suspicious

map<string, deque<Order>> user_order_window;


bool is_quote_stuffing(const string& user_id, long long now) {
    auto& dq = user_order_window[user_id];

    while (!dq.empty() && now - dq.front().timestamp > WINDOW_MS) {
        dq.pop_front();
    }

    int new_orders = 0, cancels = 0, executes = 0;
    for (const auto& o : dq) {
        if (o.order_type == "BUY" || o.order_type == "SELL") 
            new_orders++;
        else if (o.order_type == "CANCEL") 
            cancels++;
        else if (o.order_type == "EXECUTE") 
            executes++;
    }

    bool high_order_rate = new_orders > MAX_ORDERS_IN_WINDOW;
    bool high_cancel_ratio = new_orders > 0 && (cancels > 0.8 * new_orders);
    // bool low_execution_rate = new_orders > 0 && (executes < 0.1 * new_orders);

    return high_order_rate && high_cancel_ratio;
}


static void error_cb(rd_kafka_t* rk, int err, const char* reason, void* opaque) {
    fprintf(stderr, "Kafka error: %s\n", rd_kafka_err2str((rd_kafka_resp_err_t)err));
}

int main() {
    char errstr[512];

    // 🎯 Kafka consumer setup
    rd_kafka_conf_t* conf = rd_kafka_conf_new();
    rd_kafka_conf_set(conf, "bootstrap.servers", "localhost:9092", errstr, sizeof(errstr));
    rd_kafka_conf_set(conf, "group.id", "hawk-eye-quote-detector", errstr, sizeof(errstr));
    rd_kafka_conf_set(conf, "auto.offset.reset", "earliest", errstr, sizeof(errstr));
    rd_kafka_conf_set_error_cb(conf, error_cb);

    rd_kafka_t* consumer = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr, sizeof(errstr));
    rd_kafka_poll_set_consumer(consumer);

    rd_kafka_topic_partition_list_t* topics = rd_kafka_topic_partition_list_new(1);
    rd_kafka_topic_partition_list_add(topics, "order_feed", -1);
    rd_kafka_subscribe(consumer, topics);

    cout << "--- Quote Stuffing Detector started... ---\n";

    while (true) {
        rd_kafka_message_t* msg = rd_kafka_consumer_poll(consumer, 1000);
        if (!msg) continue;

        if (msg->err) {
            cerr << "Kafka error: " << rd_kafka_message_errstr(msg) << endl;
            rd_kafka_message_destroy(msg);
            continue;
        }

        string payload((char*)msg->payload, msg->len);
        Json::Value root;
        Json::CharReaderBuilder builder;
        string errs;

        auto reader = builder.newCharReader();
        if (!reader->parse(payload.c_str(), payload.c_str() + payload.size(), &root, &errs)) {
            cerr << "❌ JSON parse failed: " << errs << endl;
            rd_kafka_message_destroy(msg);
            continue;
        }

        Order o;
        o.order_id = root["order_id"].asString();
        o.order_type = root["order_type"].asString();
        o.price = root["price"].asDouble();
        o.symbol = root["symbol"].asString();
        o.quantity = root["quantity"].asInt();
        o.timestamp = stoll(root["timestamp"].asString());
        o.user_id = root["user_id"].asString();

        user_order_window[o.user_id].push_back(o);

        if (is_quote_stuffing(o.user_id, o.timestamp)) {
            cout << "--- Quote stuffing alert: ---" << o.user_id << " (" << user_order_window[o.user_id].size() << " orders in " << WINDOW_MS << "ms)" << endl;

            Json::Value alert;
            alert["user_id"] = o.user_id;
            alert["alert_type"] = "quote_stuffing";
            alert["order_id"] = o.order_id;
            alert["timestamp"] = o.timestamp;
            alert["evidence"] = "Too many orders in short window";

            Json::StreamWriterBuilder writer;
            string json_alert = Json::writeString(writer, alert);

            send_alert(json_alert);
        }

        rd_kafka_message_destroy(msg);
    }

    rd_kafka_consumer_close(consumer);
    rd_kafka_destroy(consumer);
    return 0;
}
