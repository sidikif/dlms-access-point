// ===========================================================================
// Copyright (c) 2020, Electric Power Research Institute (EPRI)
// All rights reserved.
//
// dlms-access-point ("this software") is licensed under BSD 3-Clause license.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
// *  Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// *  Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// *  Neither the name of EPRI nor the names of its contributors may
//    be used to endorse or promote products derived from this software without
//    specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
// NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
// OF SUCH DAMAGE.
//
// This EPRI software incorporates work covered by the following copyright and permission
// notices. You may not use these works except in compliance with their respective
// licenses, which are provided below.
//
// These works are provided by the copyright holders and contributors "as is" and any express or
// implied warranties, including, but not limited to, the implied warranties of merchantability
// and fitness for a particular purpose are disclaimed.
//
// This software relies on the following libraries and licenses:
//
// ###########################################################################
// Boost Software License, Version 1.0
// ###########################################################################
//
// * asio v1.10.8 (https://sourceforge.net/projects/asio/files/)
//
// Boost Software License - Version 1.0 - August 17th, 2003
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
//
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//

#include "LinuxBaseLibrary.h"
#include "LinuxCOSEMServer.h"

#include "HDLCLLC.h"
#include "COSEM.h"
#include "serialwrapper/SerialWrapper.h"
#include "tcpwrapper/TCPWrapper.h"
#include "dlms-access-pointConfig.h"

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <time.h>
#include <cctype>
#include <unistd.h>
#include <iomanip>
#include <asio.hpp>
#include <algorithm>
#include <functional>
#include <string>
#include <chrono>
#include <thread>
#include <memory>
#include <numeric>
#include <mutex>

class LinuxClientEngine : public EPRI::COSEMClientEngine
{
public:
    LinuxClientEngine() = delete;
    LinuxClientEngine(const Options& Opt, EPRI::Transport * pXPort) :
        pXPort(pXPort),
        COSEMClientEngine(Opt, pXPort)
    {
    }
    virtual ~LinuxClientEngine()
    {
        delete pXPort;
    }

    virtual bool OnOpenConfirmation(EPRI::COSEMAddressType ServerAddress)
    {
        EPRI::Base()->GetDebug()->TRACE("Associated with Server %d...\n",
            ServerAddress);
        return true;
    }

    virtual bool OnGetConfirmation(RequestToken Token, const GetResponse& Response)
    {
        EPRI::Base()->GetDebug()->TRACE("Get Confirmation for Token %d...\n", Token);
        if (Response.ResultValid && Response.Result.which() == EPRI::Get_Data_Result_Choice::data_access_result)
        {
            EPRI::Base()->GetDebug()->TRACE("\tReturned Error Code %d...\n",
                Response.Result.get<EPRI::APDUConstants::Data_Access_Result>());
            return false;
        }

        switch(Response.Descriptor.class_id) {
            case EPRI::CLSID_IData:
                {
                    EPRI::IData     SerialNumbers;
                    EPRI::DLMSValue Value;

                    SerialNumbers.value = Response.Result.get<EPRI::DLMSVector>();
                    if (EPRI::COSEMType::VALUE_RETRIEVED == SerialNumbers.value.GetNextValue(&Value))
                    {
                        recent = EPRI::DLMSValueGet<EPRI::VISIBLE_STRING_CType>(Value);
                        std::cout << "Setting recent to \"" << recent << "\"\n";
                        EPRI::Base()->GetDebug()->TRACE("%s\n", recent.c_str());
                    }

                }
                break;
            case EPRI::CLSID_IAssociationLN:
                {
                    EPRI::IAssociationLN CurrentAssociation;
                    EPRI::DLMSValue      Value;

                    switch (Response.Descriptor.attribute_id)
                    {
                    case EPRI::IAssociationLN::ATTR_PARTNERS_ID:
                        {
                            CurrentAssociation.associated_partners_id = Response.Result.get<EPRI::DLMSVector>();
                            if (EPRI::COSEMType::VALUE_RETRIEVED == CurrentAssociation.associated_partners_id.GetNextValue(&Value) &&
                                IsSequence(Value))
                            {
                                EPRI::DLMSSequence& Element = DLMSValueGetSequence(Value);
                                EPRI::Base()->GetDebug()->TRACE("ClientSAP %d; ServerSAP %d\n",
                                    EPRI::DLMSValueGet<EPRI::INTEGER_CType>(Element[0]),
                                    EPRI::DLMSValueGet<EPRI::LONG_UNSIGNED_CType>(Element[1]));
                            }
                        }
                        break;

                    default:
                        EPRI::Base()->GetDebug()->TRACE("Attribute %d not supported for parsing.", Response.Descriptor.attribute_id);
                        break;
                    }
                }
                break;
        }
        return true;
    }

    virtual bool OnSetConfirmation(RequestToken Token, const SetResponse& Response)
    {
        EPRI::Base()->GetDebug()->TRACE("Set Confirmation for Token %d...\n", Token);
        if (Response.ResultValid)
        {
            EPRI::Base()->GetDebug()->TRACE("\tResponse Code %d...\n",
                Response.Result);
        }
        return true;
    }

    virtual bool OnActionConfirmation(RequestToken Token, const ActionResponse& Response)
    {
        EPRI::Base()->GetDebug()->TRACE("Action Confirmation for Token %d...\n", Token);
        if (Response.ResultValid)
        {
            EPRI::Base()->GetDebug()->TRACE("\tResponse Code %d...\n",
                Response.Result);
        }
        return true;
    }

    virtual bool OnReleaseConfirmation()
    {
        EPRI::Base()->GetDebug()->TRACE("Release Confirmation from Server\n");
        return true;
    }

    virtual bool OnReleaseConfirmation(EPRI::COSEMAddressType ServerAddress)
    {
        EPRI::Base()->GetDebug()->TRACE("Release Confirmation from Server %d\n", ServerAddress);
        return true;
    }

    virtual bool OnAbortIndication(EPRI::COSEMAddressType ServerAddress)
    {
        if (EPRI::INVALID_ADDRESS == ServerAddress)
        {
            EPRI::Base()->GetDebug()->TRACE("Abort Indication.  Not Associated.\n");
        }
        else
        {
            EPRI::Base()->GetDebug()->TRACE("Abort Indication from Server %d\n", ServerAddress);
        }
        return true;
    }

    std::string recent_data() const {
        return recent;
    }
private:
    EPRI::Transport * pXPort{nullptr};
    std::string recent;
};


class APsim {
public:
    APsim(EPRI::LinuxBaseLibrary& bl, const std::string& meterURL, int SourceAddress = 1)
        : bl(bl)
        , m_pClientEngine{EPRI::COSEMClientEngine::Options(SourceAddress),
            new EPRI::TCPWrapper((m_pSocket = EPRI::Base()->GetCore()->GetIP()->CreateSocket(EPRI::LinuxIP::Options(EPRI::LinuxIP::Options::MODE_CLIENT, EPRI::LinuxIP::Options::VERSION6))))}
    {
        if (EPRI::SUCCESSFUL != m_pSocket->Open(meterURL.c_str()))
        {
            std::cout << "Failed to initiate connect to " << meterURL << "\n";
        }
    }
    ~APsim()
    {
        if (m_pSocket)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
            bl.get_io_service().poll();
            EPRI::Base()->GetCore()->GetIP()->ReleaseSocket(m_pSocket);
            m_pSocket = nullptr;
            std::cout << "Socket released.\n";
        }
        else
        {
            std::cout << "TCP Not Opened!\n";
        }
    }
    bool open()
    {
        static constexpr int max_tries{400};
        int tries{max_tries};
        do {
            std::this_thread::sleep_for(std::chrono::milliseconds{100});
            bl.get_io_service().poll();
            if (m_pSocket && m_pSocket->IsConnected() && m_pClientEngine.IsTransportConnected())
            {
                bool Send = true;
                int DestinationAddress = 1;
                EPRI::COSEMSecurityOptions::SecurityLevel Security = EPRI::COSEMSecurityOptions::SECURITY_NONE;
                EPRI::COSEMSecurityOptions SecurityOptions;
                SecurityOptions.ApplicationContextName = SecurityOptions.ContextLNRNoCipher;
                size_t APDUSize = 640;
                m_pClientEngine.Open(DestinationAddress,
                                      SecurityOptions,
                                      EPRI::xDLMS::InitiateRequest(APDUSize));
            }
            else
            {
                // std::cout << "Transport Connection Not Established Yet!\n";
                --tries;
            }
        } while (!(m_pSocket && m_pSocket->IsConnected() && m_pClientEngine.IsTransportConnected()) && tries > 0);
        return tries;
    }

    bool close()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
        bl.get_io_service().poll();
        if (!m_pClientEngine.Release(EPRI::xDLMS::InitiateRequest()))
        {
            std::cout << "Problem submitting COSEM Release!\n";
            return false;
        }
        return true;
    }

    bool serviceConnect(bool reconnect)
    {
        return Action(70, (reconnect ? 2 : 1), "0-0:96.3.10*255", nullptr);
    }

    bool Action(unsigned class_id, unsigned method, std::string obis, EPRI::COSEMType MyData)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
        bl.get_io_service().poll();
        if (m_pSocket && m_pSocket->IsConnected() && m_pClientEngine.IsOpen())
        {
            EPRI::Cosem_Method_Descriptor Descriptor;

            Descriptor.class_id = (EPRI::ClassIDType)class_id;
            Descriptor.method_id = (EPRI::ObjectAttributeIdType)method;
            if (Descriptor.instance_id.Parse(obis))
            {
                if (m_pClientEngine.Action(Descriptor,
                                        EPRI::DLMSOptional<EPRI::DLMSVector>(MyData),
                                        &m_ActionToken))
                {
                    PrintLine(std::string("\tAction Request Sent: Token ") + std::to_string(m_ActionToken) + "\n");
                    return true;
                }
            }
            else
            {
                PrintLine("Malformed OBIS Code!\n");
            }
        }
        else
        {
            PrintLine("Not Connected!\n");
        }
        return false;
    }

    bool Get(unsigned class_id, unsigned attribute, std::string obis)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
        bl.get_io_service().poll();
        if (m_pSocket && m_pSocket->IsConnected() && m_pClientEngine.IsOpen())
        {
            EPRI::Cosem_Attribute_Descriptor Descriptor;

            Descriptor.class_id = (EPRI::ClassIDType)class_id;
            Descriptor.attribute_id = (EPRI::ObjectAttributeIdType)attribute;
            if (Descriptor.instance_id.Parse(obis))
            {
                if (m_pClientEngine.Get(Descriptor, &m_GetToken))
                {
                    PrintLine(std::string("\tGet Request Sent: Token ") + std::to_string(m_GetToken) + "\n");
                    return true;
                }
            }
            else
            {
                PrintLine("Malformed OBIS Code!\n");
            }
        }
        else
        {
            PrintLine("Not Connected!\n");
        }
        return false;
    }

    bool Set(unsigned class_id, unsigned attribute, std::string obis, EPRI::COSEMType MyData)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
        bl.get_io_service().poll();
        if (m_pSocket && m_pSocket->IsConnected() && m_pClientEngine.IsOpen())
        {
            EPRI::Cosem_Attribute_Descriptor Descriptor;

            Descriptor.class_id = (EPRI::ClassIDType)class_id;
            Descriptor.attribute_id = (EPRI::ObjectAttributeIdType)attribute;
            if (Descriptor.instance_id.Parse(obis))
            {
                if (m_pClientEngine.Set(Descriptor, MyData, &m_SetToken))
                {
                    PrintLine(std::string("\tSet Request Sent: Token ") + std::to_string(m_SetToken) + "\n");
                    return true;
                }
            }
            else
            {
                PrintLine("Malformed OBIS Code!\n");
            }
        }
        else
        {
            PrintLine("Not Connected!\n");
        }
        return false;
    }
    void PrintLine(const std::string& str) const {
        std::cout << str;
    }
    std::string recent_data() const {
        return m_pClientEngine.recent_data();
    }
private:
    EPRI::LinuxBaseLibrary& bl;
    EPRI::ISocket* m_pSocket = nullptr;
    LinuxClientEngine m_pClientEngine;
    EPRI::COSEMClientEngine::RequestToken m_GetToken;
    EPRI::COSEMClientEngine::RequestToken m_SetToken;
    EPRI::COSEMClientEngine::RequestToken m_ActionToken;
};

class Config {
public:
    enum class Payload { small, medium, large };
    Config(const Config& other) = delete;
    Config(Config&& other) = delete;
    Config(const std::string& data) {
        if (!std::all_of(data.cbegin(), data.cend(), isprint)) {
            throw std::range_error("invalid string");
        }
        std::stringstream ss{data};
        std::string item;
        std::string plsize;
        if (std::getline(ss, plsize, ',')) {
            while (std::getline(ss, item, ',')) {
                meters_.emplace_back(item);
            }
            if (plsize == "small") {
                payload_size_ = Payload::small;
                std::swap(meters_, meters_);
            } else if (plsize == "medium") {
                payload_size_ = Payload::medium;
                std::swap(meters_, meters_);
            } else if (plsize == "large") {
                payload_size_ = Payload::large;
                std::swap(meters_, meters_);
            } else {
                throw std::range_error("invalid Payload size");
            }
        }
    }
    void interpret(const std::string& data) {
        try {
            Config other{data};
            const std::lock_guard<std::mutex> lock(mtx_);
            std::swap(meters_, other.meters_);
            std::swap(payload_size_, other.payload_size_);
        } catch (std::range_error r) {
            std::cerr << r.what() << ": " << data << '\n';
        }
    }
    std::vector<std::string> meters() const {
        const std::lock_guard<std::mutex> lock(mtx_);
        return meters_;
    }
    std::size_t count() const { 
        const std::lock_guard<std::mutex> lock(mtx_);
        return meters_.size();
    }
    const Payload payload_size() const {
        const std::lock_guard<std::mutex> lock(mtx_);
        return payload_size_;
    }
    void clear() {
        const std::lock_guard<std::mutex> lock(mtx_);
        meters_.clear();
    }
private:
    Payload payload_size_;
    std::vector<std::string> meters_{};
    mutable std::mutex mtx_;
};

/// very simple class representing a meter reading
struct MeterReading {
    std::string meterAddr;
    std::string meterData;
    friend std::ostream& operator<<(std::ostream& out, const MeterReading& mr) {
        return out << "{\"meter\":\"" << mr.meterAddr << "\",\"data\":\"" << mr.meterData << "\"}";
    }
};

std::vector<MeterReading> runScript(EPRI::LinuxBaseLibrary& bl, const Config& cfg) {
    std::vector<MeterReading> result;
    const auto meters{cfg.meters()};
    for (const auto& metername : meters) {
        std::cout << "Trying to connect to meter at " << metername << "\n";
        APsim apsim(bl, metername);
        apsim.open();
        switch (cfg.payload_size()) {
            case Config::Payload::medium:
                apsim.Get(1, 2, "0-0:96.1.4*255");
                break;
            case Config::Payload::large:
                apsim.Get(1, 2, "0-0:96.1.9*255");
                break;
            default:
                apsim.Get(1, 2, "0-0:96.1.0*255");
                break;
        }
        apsim.close();
        std::cout << "Saving " << apsim.recent_data() << "\n";
        result.emplace_back(MeterReading{metername, apsim.recent_data()});
    }
    return result;
}


using asio::ip::tcp;

class tcp_connection : public std::enable_shared_from_this<tcp_connection>
{
public:
    tcp_connection(tcp::socket socket) 
        : socket_(std::move(socket))
    {}

    void start(Config& cfg) {
        std::string remote{socket_.remote_endpoint().address().to_string()};
        socket_.async_read_some(asio::buffer(data_, max_length),
            std::bind(&tcp_connection::handle_read, this,
                std::placeholders::_1,
                std::placeholders::_2,
                std::ref(cfg))
        );
    }

    void handle_read(const std::error_code& error, size_t bytes_transferred, Config& cfg) {
        if (!error) {
            cfg.interpret(std::string{data_, bytes_transferred});
        } else {
            std::cout << "Got an error in 'handle_read()' on line " << __LINE__ << std::endl;
        }
    }

private:
    tcp::socket socket_;
    static constexpr unsigned max_length{1024};
    char data_[max_length];
};

class RegistrationServer {
public:
    RegistrationServer(asio::io_service& io_service, Config& cfg)
        : socket_{io_service}
        , acceptor_{io_service, tcp::endpoint(tcp::v6(), 4059)}
        , cfg{cfg}
    {
        std::cout << "Listening on port 4059\n";
        do_accept();
    }
private:
    void do_accept() {
        acceptor_.async_accept(socket_,
            [this](std::error_code ec) {
                if (!ec) {
                    std::make_shared<tcp_connection>(std::move(socket_))->start(cfg);
                }
                do_accept();
            });
    }

    tcp::socket socket_;
    tcp::acceptor acceptor_;
    Config& cfg;
};

void regs(Config& cfg) {
    try {
        asio::io_service io_service;
        RegistrationServer regServer(io_service, cfg);
        io_service.run();
    } catch (std::exception& err) {
        std::cerr << err.what() << '\n';
    }
}

std::ostream& operator<<(std::ostream& out, const std::vector<MeterReading>& readings) {
    out << "{\"meterdata\":[\n";
    auto it{readings.cbegin()};
    auto end{readings.cend()};
    if (it != end) {
        out << *it++;
        for ( ; it != end; ++it) {
            out << ",\n" << *it;
        }
    }
    return out << "\n]}";
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: APsim APaddress\n";
        return 1;
    }
    std::string APaddress{argv[1]};
    Config cfg("small,");
    EPRI::LinuxBaseLibrary bl;
    std::thread thr{regs, std::ref(cfg)};
    while (1) {
        std::cout << "There are " << cfg.count() << " registered meters\n";
        auto meterdata{runScript(bl, cfg)};
        std::cout << meterdata << '\n';
        cfg.clear();
        std::this_thread::sleep_for(std::chrono::milliseconds{1500});
    }
} 
