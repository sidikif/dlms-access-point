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

#pragma once

#include <asio.hpp>
#include <memory>
#include <list>

#include "ISerial.h"

namespace EPRI
{
    class LinuxSerial;
    
    class LinuxSerialSocket : public ISerialSocket
    {
        friend class LinuxIP;
        
    public:
        LinuxSerialSocket() = delete;
        LinuxSerialSocket(const ISerial::Options& Opt, asio::io_service& IO);
        virtual ~LinuxSerialSocket();
        
        ISerial::Options GetOptions();
        //
        // ISocket
        //
        virtual ERROR_TYPE Open(const char * DestinationAddress = nullptr, int Port = DEFAULT_DLMS_PORT);
        virtual ConnectCallbackFunction RegisterConnectHandler(ConnectCallbackFunction Callback);
        virtual ERROR_TYPE Write(const DLMSVector& Data, bool Asynchronous = false);
        virtual WriteCallbackFunction RegisterWriteHandler(WriteCallbackFunction Callback);
        virtual ERROR_TYPE Read(DLMSVector * pData,
            size_t ReadAtLeast = 0,
            uint32_t TimeOutInMS = 0,
            size_t * pActualBytes = nullptr);
        virtual bool AppendAsyncReadResult(DLMSVector * pData, size_t ReadAtLeast = 0);
        virtual ReadCallbackFunction RegisterReadHandler(ReadCallbackFunction Callback);
        virtual ERROR_TYPE Close();
        virtual CloseCallbackFunction RegisterCloseHandler(CloseCallbackFunction Callback);
        virtual bool IsConnected();
        //
        // ISerialSocket
        //
        virtual ERROR_TYPE Flush(FlushDirection Direction);
        virtual ERROR_TYPE SetOptions(const ISerial::Options& Opt);
        
    private:
        void ASIO_Write_Handler(const asio::error_code& Error, size_t BytesTransferred);
        void ASIO_Read_Handler(const asio::error_code& Error, size_t BytesTransferred);
        void ASIO_Read_Timeout(const asio::error_code& Error);
        void SetPortOptions();
        
        void OnClose(ERROR_TYPE Error);

        asio::serial_port               m_Port;
        asio::steady_timer              m_ReadTimer;
        asio::streambuf                 m_ReadBuffer;
        ISerial::Options                m_Options;
        ConnectCallbackFunction         m_Connect;
        WriteCallbackFunction           m_Write;
        ReadCallbackFunction            m_Read;
        CloseCallbackFunction           m_Close;
    };

    class LinuxSerial : public ISerial
    {
        friend class LinuxSerialSocket;
        
    public:
        LinuxSerial() = delete;
        LinuxSerial(asio::io_service& IO);
        virtual ~LinuxSerial();
        
        virtual ISerialSocket * CreateSocket(const Options& Opt);
        virtual void ReleaseSocket(ISerialSocket * pSocket);
        virtual bool Process();

    protected:
        void RemoveSocket(ISerialSocket * pSocket);
        
        using             SerialSocketList = std::list<LinuxSerialSocket>;        
        SerialSocketList  m_Sockets;
        asio::io_service& m_IO;
    };    

}
