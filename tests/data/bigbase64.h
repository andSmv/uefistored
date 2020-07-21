#ifndef __H_BIGBASE64_
#define __H_BIGBASE64_

#define BIG_BASE64_STR                                                                \
    "VkFSUwEAAAATAAAAAAAAAJgOAAAAAAAAKgAAAAAAAABNAGUAbQBvAHIAeQBUAHkAcABlA"           \
    "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"\
    "AAA=="

#define BIG_BASE64_XML_STR                                                                      \
    "<?xml version=\"1.0\"?>"                                                                   \
    "<methodResponse><params><param><value><struct><member><name>Status</name><value>Success</value></member><member><name>Value</name><value><struct><member><name>EFI-variables</name><value>"   \
    BIG_BASE64_STR                                                                              \
    "</value></member></struct></value></member></struct></value></param></params></methodResponse>"
#endif // __H_BIGBASE64_
