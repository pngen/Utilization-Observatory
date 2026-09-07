#pragma once
// Utilization Observatory : typed error / status model.
// Copyright 2026 Summon Software Labs. Apache-2.0.

#include <string>
#include <utility>

namespace uo {

// Typed status codes. The Success case is represented by the default-constructed
// Status which has code == Ok.
enum class StatusCode {
    Ok,
    InvalidInput,
    DuplicateObservation,
    StaleAuthority,
    StaleSource,
    StaleEvidence,
    SourceDisconnected,
    Unsupported,
    InsufficientEvidence,
    UnknownState,
    PersistenceCorrupt,
    ProtocolError,
    Overflow,
    ResourceExhausted,
    Cancelled,
    ShuttingDown,
};

const char* to_string(StatusCode c) noexcept;

class Status {
public:
    Status() noexcept = default;
    explicit Status(StatusCode code) : code_(code) {}
    Status(StatusCode code, std::string msg) : code_(code), message_(std::move(msg)) {}

    bool ok() const noexcept { return code_ == StatusCode::Ok; }
    explicit operator bool() const noexcept { return ok(); }
    StatusCode code() const noexcept { return code_; }
    const std::string& message() const noexcept { return message_; }

    friend bool operator==(const Status& a, const Status& b) noexcept {
        return a.code_ == b.code_ && a.message_ == b.message_;
    }
    friend bool operator!=(const Status& a, const Status& b) noexcept { return !(a == b); }

    static Status success() { return Status{}; }

private:
    StatusCode code_{StatusCode::Ok};
    std::string message_;
};

} // namespace uo
