#include <iostream>
#include <string>
#include <librdkafka/rdkafka.h>

void send_alert(const std::string& json_message) {
    char errstr[512];

    // Config
    rd_kafka_conf_t* conf = rd_kafka_conf_new();
    if (rd_kafka_conf_set(conf, "bootstrap.servers", "localhost:9092", errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        std::cerr << "Kafka config error: " << errstr << std::endl;
        return;
    }

    rd_kafka_t* producer = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
    if (!producer) {
        std::cerr << "Failed to create producer: " << errstr << std::endl;
        return;
    }

    const std::string topic_name = "alerts";
    rd_kafka_resp_err_t err = rd_kafka_producev(
        producer,
        RD_KAFKA_V_TOPIC(topic_name.c_str()),
        RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
        RD_KAFKA_V_VALUE(const_cast<char*>(json_message.c_str()), json_message.size()),
        RD_KAFKA_V_END
    );

    if (err != RD_KAFKA_RESP_ERR_NO_ERROR) {
        std::cerr << "Produce failed: " << rd_kafka_err2str(err) << std::endl;
    } else {
        std::cout << "--- Alert sent: --- " << json_message << std::endl;
    }

    rd_kafka_poll(producer, 0);
    rd_kafka_flush(producer, 1000);

    rd_kafka_destroy(producer); 
}
