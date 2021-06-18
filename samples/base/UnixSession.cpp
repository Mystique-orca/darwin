#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "Logger.hpp"
#include "UnixSession.hpp"

namespace darwin {

    UnixSession::UnixSession(
        boost::asio::local::stream_protocol::socket& socket,
        Manager& manager, Generator& generator) 
        : ASession(manager, generator), _connected{false}, 
            _socket{std::move(socket)},
            _filter_socket{socket.get_executor()} 
    {
        ;
    }

    void UnixSession::SetNextFilterSocketPath(std::string const& path){
        if(path.compare("no") != 0) {
            _next_filter_path = path;
            _has_next_filter = true;
        }
    }

    void UnixSession::Stop() {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("UnixSession::Stop::");
        _socket.close();
        if(_connected) {
            _filter_socket.close();
            _connected = false;
        }
    }

    void UnixSession::CloseFilterConnection() {
        if(_connected){
            _filter_socket.close();
            _connected = false;
        }
    }

    void UnixSession::ReadHeader() {
        DARWIN_LOGGER;

        DARWIN_LOG_DEBUG("ASession::ReadHeader:: Starting to read incoming header...");
        boost::asio::async_read(_socket,
                                boost::asio::buffer(_header_buffer, DarwinPacket::getMinimalSize()),
                                boost::bind(&UnixSession::ReadHeaderCallback, this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
    }

    void UnixSession::ReadBody(std::size_t size) {
        DARWIN_LOGGER;
        DARWIN_LOG_DEBUG("ASession::ReadBody:: Starting to read incoming body...");

        _socket.async_read_some(boost::asio::buffer(_body_buffer,
                                                    size),
                                boost::bind(&UnixSession::ReadBodyCallback, this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
    }

    void UnixSession::WriteToClient(std::vector<unsigned char>& packet) {
        boost::asio::async_write(_socket,
                                boost::asio::buffer(packet, packet.size()),
                                boost::bind(&UnixSession::SendToClientCallback, this,
                                            boost::asio::placeholders::error,
                                            boost::asio::placeholders::bytes_transferred));
    }

    void UnixSession::WriteToFilter(darwin_filter_packet_t* packet, size_t packet_size) {
        boost::asio::async_write(_filter_socket,
                            boost::asio::buffer(packet, packet_size),
                            boost::bind(&UnixSession::SendToFilterCallback, this,
                                        boost::asio::placeholders::error,
                                        boost::asio::placeholders::bytes_transferred));

    }

    bool UnixSession::ConnectToNextFilter() {
        DARWIN_LOGGER;
        if (!_connected) {
            DARWIN_LOG_DEBUG("UnixSession::SendToFilter:: Trying to connect to: " +
                             _next_filter_path);
            try {
                _filter_socket.connect(
                        boost::asio::local::stream_protocol::endpoint(
                                _next_filter_path.c_str()));
                _connected = true;
            } catch (std::exception const& e) {
                DARWIN_LOG_ERROR(std::string("UnixSession::SendToFilter:: "
                                             "Unable to connect to next filter: ") +
                                 e.what());
                _connected = false;
            }
        }
        return _connected;
    }
}

