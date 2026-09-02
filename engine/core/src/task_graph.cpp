#include <engine/core/task_graph.hpp>

#include <cassert>
#include <cstring>

#include <engine/core/task_scheduler.hpp>

namespace engine::core {

TaskGraph::TaskGraph(usize allocatorCapacity)
    : m_ownedAllocator(new LinearAllocator(allocatorCapacity)),
      m_allocator(m_ownedAllocator) {}

TaskGraph::TaskGraph(LinearAllocator &allocator) : m_allocator(&allocator) {}

TaskGraph::~TaskGraph() {
  reset();
  if (m_ownedAllocator) {
    delete m_ownedAllocator;
    m_ownedAllocator = nullptr;
  }
  m_allocator = nullptr;
}

TaskGraph::TaskGraph(TaskGraph &&other) noexcept
    : m_ownedAllocator(other.m_ownedAllocator), m_allocator(other.m_allocator),
      m_headNode(other.m_headNode), m_tailNode(other.m_tailNode),
      m_nodeCount(other.m_nodeCount), m_rootTasks(other.m_rootTasks),
      m_scheduler(other.m_scheduler), m_compiled(other.m_compiled) {
  other.m_ownedAllocator = nullptr;
  other.m_allocator = nullptr;
  other.m_headNode = nullptr;
  other.m_tailNode = nullptr;
  other.m_nodeCount = 0;
  other.m_rootTasks = {};
  other.m_scheduler = nullptr;
  other.m_compiled = false;
}

TaskGraph &TaskGraph::operator=(TaskGraph &&other) noexcept {
  if (this != &other) {
    reset();
    if (m_ownedAllocator) {
      delete m_ownedAllocator;
    }

    m_allocator = other.m_allocator;
    m_ownedAllocator = other.m_ownedAllocator;
    m_headNode = other.m_headNode;
    m_tailNode = other.m_tailNode;
    m_nodeCount = other.m_nodeCount;
    m_rootTasks = other.m_rootTasks;
    m_scheduler = other.m_scheduler;
    m_compiled = other.m_compiled;

    other.m_ownedAllocator = nullptr;
    other.m_allocator = nullptr;
    other.m_headNode = nullptr;
    other.m_tailNode = nullptr;
    other.m_nodeCount = 0;
    other.m_rootTasks = {};
    other.m_scheduler = nullptr;
    other.m_compiled = false;
  }
  return *this;
}

TaskNode *TaskGraph::allocateNode(const char *name) {
  assert(m_allocator != nullptr && "TaskGraph has no valid LinearAllocator");
  TaskNode *node = m_allocator->create<TaskNode>();
  assert(node != nullptr && "Failed to allocate TaskNode from LinearAllocator");

  node->m_name = name;
  node->m_graph = this;
  node->m_nextInGraph = nullptr;

  if (m_tailNode) {
    m_tailNode->m_nextInGraph = node;
    m_tailNode = node;
  } else {
    m_headNode = node;
    m_tailNode = node;
  }

  ++m_nodeCount;
  m_compiled = false;
  return node;
}

void TaskGraph::setupExecutionTask(TaskNode *node) {
  node->m_executionTask = Task<void>([node]() noexcept {
    ENGINE_PROFILE_ZONE();
#if defined(TRACY_ENABLE) || defined(ENGINE_PROFILING_ENABLE)
    if (node->m_name) {
      ZoneName(node->m_name, std::strlen(node->m_name));
    }
#endif
    node->m_userTask.execute();

    node->m_completed.store(true, std::memory_order_release);

    // Unblock successors whose incoming in-degree drops to 0
    for (SuccessorLink *link = node->m_successors; link != nullptr;
         link = link->next) {
      TaskNode *succ = link->successor;
      if (succ->m_inDegree.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        node->m_graph->onTaskReady(succ);
      }
    }

    node->m_graph->onTaskFinished();
  });
}

void TaskGraph::precede(TaskNode *predecessor, TaskNode *successor) noexcept {
  assert(predecessor != nullptr && successor != nullptr);
  assert(m_allocator != nullptr);

  SuccessorLink *link = m_allocator->create<SuccessorLink>();
  assert(link != nullptr &&
         "Failed to allocate SuccessorLink in LinearAllocator");
  link->successor = successor;
  link->next = predecessor->m_successors;
  predecessor->m_successors = link;

  successor->m_inDegree.fetch_add(1, std::memory_order_relaxed);
  successor->m_initialInDegree++;
  m_compiled = false;
}

void TaskGraph::succeed(TaskNode *successor, TaskNode *predecessor) noexcept {
  precede(predecessor, successor);
}

void TaskGraph::compile() {
  if (m_compiled) {
    return;
  }

  usize rootCount = 0;
  for (TaskNode *node = m_headNode; node != nullptr;
       node = node->m_nextInGraph) {
    node->m_inDegree.store(node->m_initialInDegree, std::memory_order_relaxed);
    node->m_completed.store(false, std::memory_order_relaxed);
    if (node->m_initialInDegree == 0) {
      ++rootCount;
    }
  }

  if (rootCount > 0) {
    m_rootTasks = m_allocator->allocateSpan<TaskNode *>(rootCount);
    assert(m_rootTasks.data() != nullptr &&
           "Failed to allocate root tasks span in LinearAllocator");

    usize idx = 0;
    for (TaskNode *node = m_headNode; node != nullptr;
         node = node->m_nextInGraph) {
      if (node->m_initialInDegree == 0) {
        m_rootTasks[idx++] = node;
      }
    }
  } else {
    m_rootTasks = {};
  }

  m_compiled = true;
}

void TaskGraph::reset() noexcept {
  // Destruct any result objects that weren't consumed
  for (TaskNode *node = m_headNode; node != nullptr;
       node = node->m_nextInGraph) {
    if (node->m_resultStorage && node->m_resultDestroyer) {
      node->m_resultDestroyer(node->m_resultStorage);
      node->m_resultStorage = nullptr;
      node->m_resultDestroyer = nullptr;
    }
    node->m_executionTask.reset();
    node->m_userTask.reset();
  }

  if (m_allocator) {
    m_allocator->reset();
  }

  m_headNode = nullptr;
  m_tailNode = nullptr;
  m_nodeCount = 0;
  m_rootTasks = {};
  m_compiled = false;
}

void TaskGraph::onTaskReady(TaskNode *node) noexcept {
  assert(m_scheduler != nullptr &&
         "TaskGraph::onTaskReady called with null scheduler");
  m_scheduler->pushTask(&node->m_executionTask);
}

void TaskGraph::onTaskFinished() noexcept {
  assert(m_scheduler != nullptr &&
         "TaskGraph::onTaskFinished called with null scheduler");
  m_scheduler->onTaskCompleted();
}

} // namespace engine::core
