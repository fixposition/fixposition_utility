/**
 * \verbatim
 * ___    ___
 * \  \  /  /
 *  \  \/  /   Copyright (c) Fixposition AG
 *  /  /\  \   All rights reserved.
 * /__/  \__\
 * \endverbatim
 *
 * @file
 * @brief Standalone ROS bag from .fpl extraction tool with no dependencies (besides ROS and standard Linux stuff)
 */

/* LIBC/STL */
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

/* EXTERNAL */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wshadow"
#include <ros/console.h>
#include <rosbag/bag.h>
#include <topic_tools/shape_shifter.h>
#pragma GCC diagnostic pop
#include <getopt.h>
#include <unistd.h>

/* PACKAGE */

/* ****************************************************************************************************************** */

class FplMessage {
   public:
    FplMessage() {}

    int Parse(const uint8_t* data, const uint32_t size) {
        if (size == 0) {
            return 0;  // nada
        }
        if (data[0] != 0xaa) {
            return 0;  // nada
        }
        if (size < 2) {
            return -1;  // wait
        }
        if (data[1] != 0x55) {
            return 0;  // nada
        }
        if (size < 8) {
            return -1;  // wait
        }
        const uint32_t payload_size = ((uint32_t)(data[7]) << 24) | ((uint32_t)(data[6]) << 16) |
                                      ((uint32_t)(data[5]) << 8) | (uint32_t)(data[4]);
        const uint32_t total_size = 8 + payload_size + (2 * sizeof(uint8_t));
        if (size < total_size) {
            return -1;  // wait
        }
        uint8_t ck1 = 0;
        uint8_t ck2 = 0;
        for (uint32_t ix = 0; ix < (total_size - 2); ix++) {
            ck1 += data[ix];
            ck2 += ck1;
        }
        if ((ck1 != data[total_size - 2]) || (ck2 != data[total_size - 1])) {
            return 0;  // nada
        }

        message_ = {data, data + total_size};
        return total_size;
    }

    const uint8_t* RawData() { return message_.data(); }
    uint32_t RawSize() { return message_.size(); }

    uint8_t Type() { return message_.size() > 8 ? message_[2] : 0; }
    uint8_t Code() { return message_.size() > 8 ? message_[3] : 0; }

    const uint8_t* PayloadData() { return message_.size() > 8 ? &message_[8] : NULL; }
    uint32_t PayloadSize() {
        return message_.size() > 8 ? ((uint32_t)(message_[7]) << 24) | ((uint32_t)(message_[6]) << 16) |
                                         ((uint32_t)(message_[5]) << 8) | (uint32_t)(message_[4])
                                   : 0;
    }

    static constexpr std::size_t MAX_MSG_SIZE = 1500000;

   private:
    std::vector<uint8_t> message_;
};

// ---------------------------------------------------------------------------------------------------------------------

class FplParser {
   public:
    FplParser() { buf_.resize(5 * FplMessage::MAX_MSG_SIZE, 0x00); }

    bool Add(const uint8_t* data, const std::size_t size) {
        if ((data == NULL) || (size == 0)) {
            return false;
        }
        if ((offs_ + size_ + size) > buf_.size()) {
            ROS_WARN("FplParser: overflow");
            return false;
        }

        std::memcpy(&buf_[offs_ + size_], data, size);
        size_ += size;
        return true;
    }

    bool Process(FplMessage& msg) {
        while (size_ > 0) {
            const int msg_size = msg.Parse(&buf_[offs_], size_);
            if (msg_size < 0) {
                return false;
            } else if (msg_size == 0) {
                offs_++;
                size_--;
            } else {
                if (offs_ > 0) {
                    ROS_WARN("FplParser: bad data");
                }
                size_ -= msg_size;
                if (size_ > 0) {
                    std::memmove(&buf_[0], &buf_[msg_size], size_);
                }
                return true;
            }
        }
        return false;
    }

   private:
    std::vector<uint8_t> buf_;
    std::size_t offs_ = 0;
    std::size_t size_ = 0;
};

// ---------------------------------------------------------------------------------------------------------------------

class FplReader {
   public:
    FplReader() { buf_.resize(FplMessage::MAX_MSG_SIZE, 0x00); };

    bool Open(const std::string& path) {
        fh_ = std::make_unique<std::ifstream>(path, std::ios::binary);
        if (fh_->fail()) {
            ROS_WARN("FplReader: fail open %s: %s", path.c_str(), std::strerror(errno));
            fh_.reset();
            return false;
        }
        ROS_DEBUG("FplReader: %s", path.c_str());
        return true;
    }

    bool Next(FplMessage& msg) {
        if (parser_.Process(msg)) {
            return true;
        }

        if (fh_) {
            fh_->read((char*)buf_.data(), buf_.size());
            const auto size = fh_->gcount();
            if (size > 0) {
                if (!parser_.Add(buf_.data(), size)) {
                    fh_.reset();
                    return false;
                }
            } else {
                fh_.reset();
                return false;
            }
        }

        return parser_.Process(msg);
    }

   private:
    std::unique_ptr<std::istream> fh_;
    FplParser parser_;
    std::vector<uint8_t> buf_;
};

// ---------------------------------------------------------------------------------------------------------------------

class BagWriter {
   public:
    BagWriter() {}

    ~BagWriter() {
        if (bag_) {
            bag_->close();
            bag_.reset();
        }
    }

    bool Open(const std::string& path, const int compress) {
        bag_ = std::make_unique<rosbag::Bag>();
        if (compress > 1) {
            bag_->setCompression(rosbag::compression::CompressionType::BZ2);
        } else if (compress > 0) {
            bag_->setCompression(rosbag::compression::CompressionType::LZ4);
        }
        try {
            bag_->open(path, rosbag::bagmode::Write);
        } catch (const rosbag::BagException& e) {
            ROS_WARN("BagWriter: fail open %s: %s", path.c_str(), e.what());
            bag_.reset();
            return false;
        }
        ROS_DEBUG("BagWriter: %s", path.c_str());
        return true;
    }

    bool AddMsgDef(const uint8_t* data, const uint32_t size) {
        const char* topic_name = (const char*)&data[0];
        const std::size_t topic_name_len = strlen(topic_name);
        const char* msg_name = (const char*)&data[topic_name_len + 1];
        const std::size_t msg_name_len = strlen(msg_name);
        const char* msg_md5 = (const char*)&data[topic_name_len + 1 + msg_name_len + 1];
        const std::size_t msg_md5_len = strlen(msg_md5);
        const char* msg_def = (const char*)&data[topic_name_len + 1 + msg_name_len + 1 + msg_md5_len + 1];
        const std::size_t msg_def_len = strlen(msg_def);
        static constexpr int MAX_LEN = 100;
        if ((topic_name_len > MAX_LEN) || (msg_name_len > MAX_LEN) || (msg_md5_len > MAX_LEN) ||
            ((topic_name_len + 1 + msg_name_len + 1 + msg_md5_len + 1 + msg_def_len + 1) > size)) {
            ROS_WARN("BagWriter: bad connection header");
            return false;
        }

        if (connection_headers_.find(topic_name) == connection_headers_.end()) {
            ROS_DEBUG("BagWriter: %s, %s, %s", topic_name, msg_name, msg_md5);
            auto hdr = boost::make_shared<ros::M_string>();
            hdr->emplace(std::string("message_definition"), msg_def);
            hdr->emplace(std::string("topic"), topic_name);
            hdr->emplace(std::string("md5sum"), msg_md5);
            hdr->emplace(std::string("type"), msg_name);
            connection_headers_.emplace(topic_name, hdr);
        }
        return true;
    }

    bool AddMsgBin(const uint8_t* data, const uint32_t size) {
        if (!bag_) {
            return false;
        }

        ros::Time rec_time;
        std::memcpy(&rec_time.sec, &data[0], sizeof(rec_time.sec));
        std::memcpy(&rec_time.nsec, &data[sizeof(rec_time.sec)], sizeof(rec_time.nsec));

        const uint32_t name_offs = sizeof(rec_time.sec) + sizeof(rec_time.nsec);
        const std::string topic_name{(const char*)&data[name_offs]};
        if (topic_name.size() > 100) {
            ROS_WARN("BagWriter: bad ros message");
            return false;
        }

        auto conn_hdr_entry = connection_headers_.find(topic_name);
        if (conn_hdr_entry == connection_headers_.end()) {
            ROS_WARN("BagWriter: missing connection headers for %s", topic_name.c_str());
            return false;
        }

        struct ShapeShifterReadHelper {
            ShapeShifterReadHelper(const uint8_t* data, const uint32_t size) : data_{data}, size_{size} {}
            const uint8_t* data_;
            const uint32_t size_;
            std::size_t getLength() { return size_; }
            const uint8_t* getData() { return data_; }
        };

        const uint32_t msg_offs = name_offs + topic_name.size() + 1;
        const uint8_t* msg_bin = &data[msg_offs];
        const uint32_t msg_size = size - msg_offs;
        topic_tools::ShapeShifter ros_msg;
        ShapeShifterReadHelper stream(msg_bin, msg_size);
        ros_msg.read(stream);

        try {
            bag_->write(topic_name, rec_time, ros_msg, conn_hdr_entry->second);
        } catch (std::exception& e) {
            ROS_WARN("BagWriter write fail: %s", e.what());
            return false;
        }

        return true;
    }

   private:
    std::unique_ptr<rosbag::Bag> bag_;
    std::map<std::string, boost::shared_ptr<ros::M_string>> connection_headers_;
};

// ---------------------------------------------------------------------------------------------------------------------

int main(int argc, char** argv) {
    // Initialise ROS
    ros::Time::init();

    // Defaults
    int compress = false;
    bool overwrite = false;
    int verbosity = 0;
    std::string input_fpl;
    std::string output_bag;

    // Get command line options
    bool ok = true;
    while (true) {
        // Command line options
        static const struct option long_opts[] = {
            // clang-format off
            { "help",            no_argument,       NULL, 'h' },
            { "verbose",         no_argument,       NULL, 'v' },
            { "quiet",           no_argument,       NULL, 'q' },
            { "input",           required_argument, NULL, 'i' },
            { "output",          required_argument, NULL, 'o' },
            { "force",           no_argument,       NULL, 'f' },
            { "compress",        no_argument,       NULL, 'c' },
            { NULL, 0, NULL, 0 },
        };  // clang-format on
        const char* short_opts = ":hvqi:o:fc";

        // Process all command line options
        int opt_ix = 0;
        const int opt = getopt_long(argc, argv, short_opts, long_opts, &opt_ix);
        if (opt < 0) {
            break;
        }
        switch (opt) {
            case 'h':
                std::fputs(
                    "Usage:\n"
                    "\n"
                    "    fpl2bag [-v] [-q] [-f] [-c] -i <input_fpl> -o <output_bag>\n"
                    "\n"
                    "Where:\n"
                    "\n"
                    "    -i <input_fpl>  -- Path to input .fpl file\n"
                    "    -o <output_bag> -- Path to output .bag file\n"
                    "    -v / -q         -- Increase / decrease verbosity\n"
                    "    -f              -- Force overwrite existing output\n"
                    "    -c              -- Compress output bag, -c -c -- compress more\n"
                    "\n",
                    stdout);
                exit(EXIT_SUCCESS);
                break;
            case 'v':
                verbosity++;
                break;
            case 'q':
                verbosity--;
                break;
            case 'f':
                overwrite = true;
                break;
            case 'c':
                compress++;
                break;
            case 'i':
                input_fpl = optarg;
                break;
            case 'o':
                output_bag = optarg;
                break;
            // Special getopt_long() cases
            case '?':
                ROS_WARN("Invalid option '-%c'", optopt);
                ok = false;
                break;
            case ':':
                ROS_WARN("Missing argument to option '-%c'", optopt);
                ok = false;
                break;
        }
    }

    // Setup debugging
    if (verbosity != 0) {
        ros::console::Level level = ros::console::levels::Count;
        if (verbosity < 0) {
            level = ros::console::levels::Warn;
        } else if (verbosity > 0) {
            level = ros::console::levels::Debug;
        }
        // Set ROS and librtknav logging level
        if (level != ros::console::levels::Count) {
            if (ros::console::set_logger_level(ROSCONSOLE_DEFAULT_NAME, level) &&
                ros::console::set_logger_level("ros.extract", level)) {
                ros::console::notifyLoggerLevelsChanged();
            }
        }
    }

    // Check arguments
    if (input_fpl.empty() || output_bag.empty()) {
        ROS_WARN("Missing arguments");
        ok = false;
    }
    if (!output_bag.empty() && !overwrite && (access(output_bag.c_str(), F_OK) == 0)) {
        ROS_WARN("Output bag %s already exists", output_bag.c_str());
        ok = false;
    }

    // Are we happy?
    if (!ok) {
        ROS_INFO("Try 'extract -h'...");
        exit(EXIT_FAILURE);
    }

    // We should be good to go
    ROS_INFO("Extracting %s to %s", input_fpl.c_str(), output_bag.c_str());

    // Open input
    FplReader reader;
    if (!reader.Open(input_fpl)) {
        ok = false;
    }

    // Open output
    BagWriter writer;
    if (!writer.Open(output_bag, compress)) {
        ok = false;
    }

    // Process input data, write output bag
    FplMessage fpl_msg;
    while (ok && reader.Next(fpl_msg)) {
        const auto type = fpl_msg.Type();
        const auto code = fpl_msg.Code();
        if (code == 1) {
            if (type == 1) {
                if (!writer.AddMsgDef(fpl_msg.PayloadData(), fpl_msg.PayloadSize())) {
                    ok = false;
                }
            } else if (type == 2) {
                if (!writer.AddMsgBin(fpl_msg.PayloadData(), fpl_msg.PayloadSize())) {
                    ok = false;
                }
            }
        }
    }

    if (ok) {
        ROS_INFO("Done");
        return (EXIT_SUCCESS);
    } else {
        ROS_ERROR("Failed");
        return (EXIT_FAILURE);
    }
}

/* ****************************************************************************************************************** */
