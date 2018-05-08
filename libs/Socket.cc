#include "Socket.h"
#include <cstdio>

// ============================= Base Socket ===========================================
BaseSocket::BaseSocket(int socketFd) : socketFd(socketFd) {
  if (socketFd == invalidSocketFd) {
    ErrorLog::BaseSocketError("Constructor, Error on initializing socket file descriptor");
  }
}

BaseSocket::~BaseSocket() {
  close();
}

void BaseSocket::close() {
  if (::close(socketFd) < 0) {
    ErrorLog::BaseSocketError("Error on closing socket file descriptor");
  }
}

// ============================= Data Socket ===========================================
std::string DataSocket::receiveMessage() {
  char recvMsg[MAX_BUFF_SIZE];
  ::memset(recvMsg, 0, MAX_BUFF_SIZE);
  std::size_t n;
  n = ::read(getSocketFd(), recvMsg, MAX_BUFF_SIZE);
  if (n < 0)
    ErrorLog::DataSocketError("error occuring when receiving data");
  return std::string(recvMsg);
}

void DataSocket::sendMessage(const std::string& msg) {
  char const* buffer = msg.c_str();
  std::size_t size = msg.size();
  std::size_t dataWritten = 0;

  while(dataWritten < size) {
    std::size_t put = ::write(getSocketFd(), buffer + dataWritten, size - dataWritten);

    if (put == static_cast<std::size_t>(-1)) {
      switch(errno) {
        case EPIPE:
        {
            // fatal error
          ErrorLog::DataSocketError("error occuring when sending data");
        }
        case EAGAIN:
        {
            // temporary error, retry writing data
            continue;
        }
      }
    }

    dataWritten += put;
  }  
}

bool DataSocket::sendFile(const std::string& file) {
  FILE* fin = std::fopen(file.c_str(), "rb");
  if (!fin) {
    // todo
    std::cout << "ERROR OPEN FILE!";
    return false;
  }

  ssize_t bytesRead = 0, r;
  char buffer[MAX_BUFF_SIZE];
  while ((r = read(fileno(fin), buffer, MAX_BUFF_SIZE)) > 0) {
    bytesRead += r;
    ssize_t bytesWritten = 0, w;
    while (r > 0 && (w = write(getSocketFd(), buffer + bytesWritten, r)) > 0) {
      bytesWritten +=  w;
      r -= w;
    }
    if (w < 0) {
      // todo handle error
    }
  }
  fclose(fin);
  return true;
}

// ============================= Host Socket ===========================================
HostSocket::HostSocket(sockaddr_in myAddr, unsigned short port /* = 0 */) : BaseSocket(::socket(AF_INET, SOCK_STREAM, 0)) {
  const int MAX_CONNECTIONS_ALLOWED = 1; 
  /* ::memset((char*)&_host_addr, 0, sizeof(_host_addr)); */
  /* _host_addr.sin_family = AF_INET; */
  /* _host_addr.sin_addr.s_addr = INADDR_ANY; */
  _host_addr = myAddr;
  _host_addr.sin_port = htons(port);

  if (::bind(getSocketFd(), (sockaddr *)&_host_addr, sizeof(_host_addr)) != 0) {  
    close();
    ErrorLog::HostSocketError("Error on binding socket");
  }

  if (::listen(getSocketFd(), MAX_CONNECTIONS_ALLOWED) != 0) {
    close();
    ErrorLog::HostSocketError("Error on listening");
  }

  socklen_t len = sizeof(myAddr);
  if (::getsockname(getSocketFd(), (sockaddr*)&myAddr, &len) < 0) {
    close();
    ErrorLog::ConnectSocketError("getsockname");
  }
  _port = myAddr.sin_port;
  _addr = myAddr.sin_addr.s_addr;
}

DataSocket HostSocket::accept() {
  struct sockaddr_storage serverStorage;
  socklen_t addr_size = sizeof serverStorage;
  int newSocket = ::accept(getSocketFd(), (struct sockaddr*)&serverStorage, &addr_size);
  if (newSocket == -1) {
    ErrorLog::HostSocketError("Error on accepting");
  }
  return DataSocket(newSocket);
}

// ============================= Connect Socket ===========================================
ConnectSocket::ConnectSocket(const std::string& host, unsigned int port) : DataSocket(::socket(AF_INET, SOCK_STREAM, 0)) {
  _host = ::gethostbyname(host.c_str());
  if (_host == NULL) {
    errno = ECONNREFUSED;
    ErrorLog::ConnectSocketError("No such host");
  }

  ::memset((char*) &_host_addr, 0, sizeof(_host_addr));
  _host_addr.sin_family = AF_INET;
  ::memcpy((char*) &_host_addr.sin_addr.s_addr, (char*) _host->h_addr , _host->h_length);
  _host_addr.sin_port = htons(port);

  if (::connect(getSocketFd(), (sockaddr*) &_host_addr, sizeof(_host_addr)) != 0) {
    close();
    ErrorLog::ConnectSocketError("Can't connect to host");
  }

  socklen_t len = sizeof(_myAddr);
  if (::getsockname(getSocketFd(), (sockaddr*)&_myAddr, &len) < 0) {
    close();
    ErrorLog::ConnectSocketError("getsockname");
  }
}

// ============================= Log ===========================================
using namespace ErrorLog;

void ErrorLog::error(const std::string& msg) {
  std::cout << msg << ": " << std::strerror(errno) << '\n';
  exit(EXIT_FAILURE);
}

void ErrorLog::BaseSocketError(const std::string& msg) {
  error("[BaseSocket] " + msg);
}

void ErrorLog::DataSocketError(const std::string& msg) {
  error("[DataSocket] " + msg);
}

void ErrorLog::HostSocketError(const std::string& msg) {
   error("[HostSocket] " + msg);
}

void ErrorLog::ConnectSocketError(const std::string& msg) {
   error("[ConnectSocket] " + msg);
}