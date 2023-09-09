#pragma once

class EventProcessor : public RE::BSTEventSink<RE::TESHitEvent>,
                           public RE::BSTEventSink<RE::InputEvent*> {
    // Code from https://youtube.com/SkyrimScripting
    EventProcessor() = default;
    ~EventProcessor() = default;
    EventProcessor(const EventProcessor&) = delete;
    EventProcessor(EventProcessor&&) = delete;
    EventProcessor& operator=(const EventProcessor&) = delete;
    EventProcessor& operator=(EventProcessor&&) = delete;

public:
    static EventProcessor& GetSingleton();
    RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* event,
                                              RE::BSTEventSource<RE::TESHitEvent>*) override;
    RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* eventPtr, RE::BSTEventSource<RE::InputEvent*>*);
};

