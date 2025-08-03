#include <iostream>
#include <string>
#include <map>
#include <queue>
#include <chrono>
#include <json/json.h>
#include <librdkafka/rdkafka.h>

using namespace std;

extern void send_alert(const std::string& json_message); // func decl - will call send_alert() function in kafka_producer.cpp

struct Order {
    string order_id;
    string order_type;
    double price;
    int quantity;
    long long timestamp;
    string user_id;
};

map<string, queue<Order>> order_book; // userid, orders of json

// Spoofing threshold parameters
const int ORDER_WINDOW_MS = 5000;
const int MIN_CANCELLED_ORDERS = 3; // included upper limit for cancellation

bool is_spoofing(const string& user_id, long long current_time, int &cancel_count, int &total) {
    auto& orders = order_book[user_id];

    while (!orders.empty() && (current_time - orders.front().timestamp > ORDER_WINDOW_MS)) {
        orders.pop();
    }

    // Count cancel orders in window
    auto temp_order = orders;
    while(!temp_order.empty()) {
        total++;
        if (temp_order.front().order_type == "CANCEL") 
            cancel_count++;
        temp_order.pop();
    }

    return total >= MIN_CANCELLED_ORDERS && ((double)cancel_count / total) > 0.7;
}

// Kafka error callback
static void error_cb(rd_kafka_t* rk, int err, const char* reason, void* opaque) {
    fprintf(stderr, "Kafka error: %s\n", rd_kafka_err2str((rd_kafka_resp_err_t)err));
}

int main() {
    char errstr[512];

    // Kafka consumer config
    rd_kafka_conf_t* conf = rd_kafka_conf_new();
    rd_kafka_conf_set(conf, "bootstrap.servers", "localhost:9092", errstr, sizeof(errstr));
    rd_kafka_conf_set(conf, "group.id", "hawk-eye-consumer", errstr, sizeof(errstr));
    rd_kafka_conf_set(conf, "auto.offset.reset", "earliest", errstr, sizeof(errstr));
    rd_kafka_conf_set(conf, "enable.auto.commit", "true", errstr, sizeof(errstr));
    rd_kafka_conf_set_error_cb(conf, error_cb);

    rd_kafka_t* consumer = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr, sizeof(errstr));
    rd_kafka_poll_set_consumer(consumer);

    rd_kafka_topic_partition_list_t* topics = rd_kafka_topic_partition_list_new(1);
    rd_kafka_topic_partition_list_add(topics, "order_feed", -1);
    rd_kafka_subscribe(consumer, topics);

    cout << "--- Spoofing detector started... ---" << endl;

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
            cerr << "JSON parse error: " << errs << endl;
            continue;
        }

        Order o;
        o.order_id = root["order_id"].asString();
        o.order_type = root["order_type"].asString();
        o.price = root["price"].asDouble();
        o.quantity = root["quantity"].asInt();
        o.timestamp = stoll(root["timestamp"].asString());
        o.user_id = root["user_id"].asString();

        order_book[o.user_id].push(o);

        int cancel_count = 0, total = 0;
        if (is_spoofing(o.user_id, o.timestamp, cancel_count, total)) {
            Json::Value alert;
            alert["user_id"] = o.user_id;
            alert["alert_type"] = "spoofing";
            alert["order_id"] = o.order_id;
            alert["timestamp"] = o.timestamp;
            alert["evidence"] = "cancel/total = " + std::to_string(cancel_count) + "/" + std::to_string(total);

            Json::StreamWriterBuilder builder;
            std::string json_alert = Json::writeString(builder, alert);
            
            cout << "--- Spoofing alert: ---" << json_alert << endl;

            send_alert(json_alert);
        }

        rd_kafka_message_destroy(msg);
    }

    rd_kafka_consumer_close(consumer);
    rd_kafka_destroy(consumer);
    return 0;
}
