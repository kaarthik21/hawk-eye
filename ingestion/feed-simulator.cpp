#include <iostream>
#include <string>
#include <random>
#include <chrono>
#include <thread>
#include <sstream>
#include <json/json.h>               
#include <librdkafka/rdkafka.h>

const char* TOPIC_NAME = "order_feed";

std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  now.time_since_epoch()).count();
    return std::to_string(ms);
}

std::string random_user() {
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(1000, 9999);
    return "user_" + std::to_string(dist(rng));
}

std::string random_order_type() {
    static const std::string types[] = {"BUY", "SELL", "CANCEL"};
    return types[rand() % 3];
}

double random_price() {
    return 100.0 + (rand() % 1000) / 10.0;  // 100.0 to 200.0
}

int random_quantity() {
    return (rand() % 10 + 1) * 100;  // Multiples of 100
}

std::string generate_order_json() {
    Json::Value order;
    order["timestamp"] = get_timestamp();
    order["user_id"] = random_user();
    order["order_type"] = random_order_type();
    order["price"] = random_price();
    order["quantity"] = random_quantity();
    order["order_id"] = "ORD" + std::to_string(rand() % 100000);
    
    Json::StreamWriterBuilder writer;
    return Json::writeString(writer, order);
}

void produce_event(rd_kafka_t* producer, rd_kafka_topic_t* topic, const std::string& msg) {
    rd_kafka_produce(
        topic,
        RD_KAFKA_PARTITION_UA,
        RD_KAFKA_MSG_F_COPY,
        const_cast<char*>(msg.c_str()), msg.size(),
        nullptr, 0,
        nullptr
    );
    rd_kafka_poll(producer, 0);  // Serve delivery report callbacks
}

int main() {
    char errstr[512];
    rd_kafka_conf_t* conf = rd_kafka_conf_new();
    
    rd_kafka_conf_set(conf, "bootstrap.servers", "localhost:9092", errstr, sizeof(errstr));

    rd_kafka_t* producer = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
    if (!producer) {
        std::cerr << "Failed to create producer: " << errstr << "\n";
        return 1;
    }

    rd_kafka_brokers_add(producer, "localhost:9092");
    rd_kafka_topic_t* topic = rd_kafka_topic_new(producer, TOPIC_NAME, nullptr);

    std::cout << "🟢 Feed simulator started — pushing events to Kafka\n";

    while (true) {
        std::string order_json = generate_order_json();
        produce_event(producer, topic, order_json);
        std::cout << "📤 Sent: " << order_json << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(500));  // 2 orders/sec
    }

    rd_kafka_topic_destroy(topic);
    rd_kafka_destroy(producer);
    return 0;
}
