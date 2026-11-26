// The AlertService engine is implemented header-only in include/team/alert_service.h
// so src/main.cpp, tests/test_app.cpp, and benchmarks/harness.cpp can all instantiate
// AlertService<TopicStore, PriceIndex, Scheduler, InboxBuffer> directly.
