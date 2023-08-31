/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifdef MVFST_USE_LIBEV

#include <glog/logging.h>
#include <quic/common/QuicEventBase.h>

namespace quic {

namespace {
void libEvTimeoutCallback(
    struct ev_loop* /* loop */,
    ev_timer* w,
    int /* revents */) {
  auto asyncTimeout = static_cast<QuicAsyncTimeout*>(w->data);
  if (asyncTimeout) {
    asyncTimeout->timeoutExpired();
  }
}
} // namespace

QuicAsyncTimeout::QuicAsyncTimeout(QuicLibevEventBase* evb) {
  CHECK(evb != nullptr);
  eventBase_ = evb;
  timeoutWatcher_.data = this;
}

QuicAsyncTimeout::~QuicAsyncTimeout() {
  ev_timer_stop(eventBase_->getLibevLoop(), &timeoutWatcher_);
}

void QuicAsyncTimeout::scheduleTimeout(double seconds) {
  ev_timer_init(
      &timeoutWatcher_,
      libEvTimeoutCallback,
      seconds /* after */,
      0. /* repeat */);
  ev_timer_start(eventBase_->getLibevLoop(), &timeoutWatcher_);
}

void QuicAsyncTimeout::cancelTimeout() {
  ev_timer_stop(eventBase_->getLibevLoop(), &timeoutWatcher_);
}

void QuicEventBase::setBackingEventBase(QuicLibevEventBase* evb) {
  backingEvb_ = evb;
}

QuicLibevEventBase* QuicEventBase::getBackingEventBase() const {
  return backingEvb_;
}

void QuicEventBase::runInLoop(
    QuicEventBaseLoopCallback* callback,
    bool thisIteration) {
  return backingEvb_->runInLoop(callback, thisIteration);
}

void QuicEventBase::runInLoop(folly::Function<void()> cb, bool thisIteration) {
  return backingEvb_->runInLoop(std::move(cb), thisIteration);
}

void QuicEventBase::runAfterDelay(
    folly::Function<void()> cb,
    uint32_t milliseconds) {
  return backingEvb_->runAfterDelay(std::move(cb), milliseconds);
}

void QuicEventBase::runInEventBaseThreadAndWait(
    folly::Function<void()> fn) noexcept {
  return backingEvb_->runInEventBaseThreadAndWait(std::move(fn));
}

bool QuicEventBase::isInEventBaseThread() const {
  return backingEvb_->isInEventBaseThread();
}

bool QuicEventBase::scheduleTimeoutHighRes(
    QuicAsyncTimeout* obj,
    std::chrono::microseconds timeout) {
  return backingEvb_->scheduleTimeoutHighRes(obj, timeout);
}

bool QuicEventBase::loopOnce(int flags) {
  return backingEvb_->loopOnce(flags);
}

bool QuicEventBase::loop() {
  return backingEvb_->loop();
}

void QuicEventBase::loopForever() {
  return backingEvb_->loopForever();
}

bool QuicEventBase::loopIgnoreKeepAlive() {
  return backingEvb_->loopIgnoreKeepAlive();
}

void QuicEventBase::terminateLoopSoon() {
  return backingEvb_->terminateLoopSoon();
}

void QuicEventBase::scheduleTimeout(
    QuicTimerCallback* callback,
    std::chrono::milliseconds timeout) {
  return backingEvb_->scheduleTimeout(callback, timeout);
}

std::chrono::milliseconds QuicEventBase::getTimerTickInterval() const {
  return backingEvb_->getTimerTickInterval();
}

QuicLibevEventBase::QuicLibevEventBase(struct ev_loop* loop) : ev_loop_(loop) {}

void QuicLibevEventBase::runInLoop(
    folly::Function<void()> cb,
    bool /* thisIteration */) {
  cb();
}

void QuicLibevEventBase::runInLoop(
    QuicEventBaseLoopCallback* callback,
    bool /* thisIteration */) {
  callback->runLoopCallback();
}

void QuicLibevEventBase::runInEventBaseThreadAndWait(
    folly::Function<void()> fn) noexcept {
  fn();
}

bool QuicTimer::Callback::isScheduled() const {
  return asyncTimeout_ != nullptr;
}

void QuicTimer::Callback::cancelTimeout() {
  if (asyncTimeout_) {
    asyncTimeout_->cancelTimeout();
    asyncTimeout_.reset();
  }
}

void QuicTimer::Callback::setAsyncTimeout(
    std::unique_ptr<QuicAsyncTimeout> asyncTimeout) {
  CHECK(!asyncTimeout_);
  asyncTimeout_ = std::move(asyncTimeout);
}

class EvTimer : public QuicAsyncTimeout {
 public:
  EvTimer(QuicLibevEventBase* evb, QuicTimer::Callback* cb)
      : QuicAsyncTimeout(evb), cb_(cb) {}

  void timeoutExpired() noexcept override {
    cb_->timeoutExpired();
  }

 private:
  QuicTimer::Callback* cb_;
};

void QuicLibevEventBase::scheduleTimeout(
    QuicTimer::Callback* callback,
    std::chrono::milliseconds timeout) {
  auto evTimer = std::make_unique<EvTimer>(this, callback);
  evTimer->scheduleTimeout(timeout.count() / 10000.);
  callback->setAsyncTimeout(std::move(evTimer));
}
} // namespace quic

#endif // MVFST_USE_LIBEV
