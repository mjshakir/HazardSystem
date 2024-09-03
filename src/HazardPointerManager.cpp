//--------------------------------------------------------------
// Main Header 
//--------------------------------------------------------------
#include "HazardSystem/HazardPointerManager.hpp"
//--------------------------------------------------------------
// Standard cpp library
//--------------------------------------------------------------
#include <thread>
#include <vector>
//--------------------------------------------------------------
// User Defined Headers
//--------------------------------------------------------------
#include "HazardSystem/HazardPointer.hpp"
//--------------------------------------------------------------
constexpr size_t HAZARD_POINTERS_PER_THREAD  = 2UL; // Number of hazard pointers per thread and total hazard pointers
static thread_local std::vector<CircularBuffer::HazardSystem::HazardPointer*> m_thread_hazard_pointers;
//--------------------------------------------------------------
CircularBuffer::HazardSystem::HazardPointerManager::HazardPointerManager(const size_t retries) 
                            :   m_num_threads(!std::thread::hardware_concurrency() ? 1 : static_cast<size_t>(std::thread::hardware_concurrency())),
                                m_max_retired(retries) { 
    //--------------------------
    m_hazard_pointers.fill(nullptr);
    //--------------------------
    const size_t num_hazard_pointers    = static_cast<size_t>(HAZARD_POINTERS / std::thread::hardware_concurrency());
    const size_t _hazard_pointers       = std::clamp(num_hazard_pointers, HAZARD_POINTERS_PER_THREAD, HAZARD_POINTERS);
    m_thread_hazard_pointers.reserve(_hazard_pointers);
    //--------------------------
}// end HazardPointerManager(void)
//--------------------------------------------------------------
CircularBuffer::HazardSystem::HazardPointerManager::~HazardPointerManager(void) {
    //--------------------------
    for (auto& hp : m_hazard_pointers) {
        //--------------------------
        if (hp.load()) {
            delete hp.load();  // Properly clean up allocated memory
        } // end if (hp.load())
        //--------------------------
        hp.store(nullptr);
        //--------------------------
    } // end for (auto& hp : m_hazard_pointers)
    //--------------------------
}// end ~HazardPointerManager(void)
//--------------------------------------------------------------
CircularBuffer::HazardSystem::HazardPointerManager& CircularBuffer::HazardSystem::HazardPointerManager::instance(void) {
    //--------------------------
    static HazardPointerManager instance;
    return instance;
    //--------------------------
}// end static HazardPointerManager& instance(void)
CircularBuffer::HazardSystem::HazardPointer* CircularBuffer::HazardSystem::HazardPointerManager::acquire(void) {
    //--------------------------
    return acquire_data();
    //--------------------------
}// end std::optional<HazardPointer*> acquire(void)
//--------------------------------------------------------------
// Release a hazard pointer slot
void CircularBuffer::HazardSystem::HazardPointerManager::release(HazardPointer* hp) {
    //--------------------------
    release_data(hp);
    //--------------------------
}// end void release(HazardPointer* hp)
//--------------------------------------------------------------
// Check if any hazard pointer is pointing to the given object
bool CircularBuffer::HazardSystem::HazardPointerManager::hazard(void* ptr) const {
    //--------------------------
    return is_hazard(ptr);
    //--------------------------
}// end bool hazard(void* ptr) const
//--------------------------------------------------------------
void CircularBuffer::HazardSystem::HazardPointerManager::retire(void* node, std::function<void(void*)> deleter) {
    //--------------------------
    retire_node(node, deleter);
    //--------------------------
}// end void retire(void* node, std::function<void(void*)> deleter)
//--------------------------------------------------------------
void CircularBuffer::HazardSystem::HazardPointerManager::reclaim(void) {
    //--------------------------
    scan_and_reclaim();
    //--------------------------
}// end void reclaim(void)
//--------------------------------------------------------------
CircularBuffer::HazardSystem::HazardPointer* CircularBuffer::HazardSystem::HazardPointerManager::acquire_data(void) {
    //--------------------------
    // First, try to find a free hazard pointer in the thread-local storage
    for (auto& hp : m_thread_hazard_pointers) {
        if (hp and hp->pointer.load(std::memory_order_acquire) == nullptr) {
            return hp;
        } // end if (hp && hp->pointer.load(std::memory_order_acquire) == nullptr)
    } // end for (auto& hp : m_thread_hazard_pointers)
    //--------------------------
    // If none is free in the thread-local storage, acquire a new one from the global pool
    for (auto& hp : m_hazard_pointers) {
        //--------------------------
        HazardPointer* expected = nullptr;
        if (hp.compare_exchange_strong(expected, nullptr, std::memory_order_acquire, std::memory_order_relaxed)) {
            //--------------------------
            if (!expected) {
                expected = new HazardPointer();
                hp.store(expected, std::memory_order_release);
            } // end if (!expected)
            //--------------------------
            // m_thread_hazard_pointers.at(0) = expected;    // Assuming single hazard pointer per thread for simplicity
            m_thread_hazard_pointers.push_back(expected);
            //--------------------------
            return expected;
            //--------------------------
        } // end if (hp.compare_exchange_strong(expected, desired, std::memory_order_acquire, std::memory_order_relaxed))
    } // end for (auto& hp : m_hazard_pointers)
    //--------------------------
    return nullptr; // Explicitly return nullptr if no available hazard pointers
    //--------------------------
}// end std::optional<HazardPointer*> acquire_data(void)
//--------------------------------------------------------------
void CircularBuffer::HazardSystem::HazardPointerManager::release_data(HazardPointer* hp) {
    //--------------------------
    hp->pointer.store(nullptr, std::memory_order_release);
    //--------------------------
}// end void release_data(HazardPointer* hp)
//--------------------------------------------------------------
bool CircularBuffer::HazardSystem::HazardPointerManager::is_hazard(void* ptr) const {
    //--------------------------
    for (const auto& hp : m_hazard_pointers) {
        if (hp.load() and hp.load()->pointer.load() == ptr) {
            return true;
        } // end if (hp.load() and hp.load()->pointer.load() == ptr)
    } // end for (const auto& hp : m_hazard_pointers)
    //--------------------------
    return false;
    //--------------------------
}// end bool is_hazard(void* ptr) const
//--------------------------------------------------------------
void CircularBuffer::HazardSystem::HazardPointerManager::retire_node(void* node, std::function<void(void*)> deleter) {
    //--------------------------
    std::lock_guard<std::mutex> lock(m_mutex);
    //--------------------------
    m_retired_nodes.emplace(node, deleter);
    //--------------------------
    if (m_retired_nodes.size() >= m_max_retired) {
        scan_and_reclaim();
    } // end if (m_retired_nodes.size() >= m_max_retired)
    //--------------------------
}// end void retire_node(void* node, std::function<void(void*)> deleter)
//--------------------------------------------------------------
void CircularBuffer::HazardSystem::HazardPointerManager::scan_and_reclaim(void) {
    //--------------------------
    for (auto it = m_retired_nodes.begin(); it != m_retired_nodes.end();) {
        //--------------------------
        if (!is_hazard(it->first)) {
            it->second(it->first);  // Call the deleter
            it = m_retired_nodes.erase(it);  // Erase after calling deleter
        } else {
            ++it;
        }
        //--------------------------
    } // end for (auto it = m_retired_nodes.begin(); it != m_retired_nodes.end();)
    //--------------------------
}// end void scan_and_reclaim(void)
//--------------------------------------------------------------