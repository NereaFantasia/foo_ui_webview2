#pragma once
#include "keyValueIO.h"

#include <string>
#include <unordered_map>

namespace fb2k {
    class keyValueIOimpl : public keyValueIO {
    public:
        fb2k::stringRef get(const char * name) override {
            auto i = m_content.find(name);
            if (i == m_content.end()) return fb2k::makeString("");
            return fb2k::makeString(i->second.c_str());
        }
        void put(const char * name, const char * value) override {
            m_content[name] = value;
        }
        // OVERRIDE ME
        void commit() override {}
        // OVERRIDE ME
        void reset() override {}
        // OVERRIDE ME
        void dismiss(bool bOK) override {std::ignore=bOK;}

    protected:
        std::unordered_map<std::string, std::string> m_content;
    };
}
