
#ifndef VEHICLE_QUEUE_H
#define VEHICLE_QUEUE_H

#include "vehicle.h"

#include <cstddef>

class VehicleQueue {
private:
    struct Node {
        Vehicle* vehicle;
        Node* next;

        explicit Node(Vehicle* v) : vehicle(v), next(nullptr) {}
    };

    Node* normalFront;
    Node* normalRear;
    Node* emergencyFront;
    Node* emergencyRear;

    std::size_t currentSize;
    std::size_t capacity;
    std::size_t blockedCount;
    std::size_t maxSize;

    void clearNodes(Node*& front, Node*& rear);

public:
    explicit VehicleQueue(std::size_t maxCapacity);

    ~VehicleQueue();

    VehicleQueue(const VehicleQueue&) = delete;
    VehicleQueue& operator=(const VehicleQueue&) = delete;

    bool enqueue(Vehicle* vehicle);
    Vehicle* dequeue();
    Vehicle* front() const;

    bool isEmpty() const;
    bool isFull() const;
    std::size_t size() const;

    std::size_t getCapacity() const;
    std::size_t getBlockedCount() const;
    std::size_t getMaxSize() const;

    void clear();
};

#endif
