// SPDX-License-Identifier: MIT
// Copyright (c) 2024-2026 Alex Conesa

#include "connection_manager.h"

#include <emblogx/logger.h>

namespace ungula::net::connection
{

ConnectionManager::ConnectionManager(ISessionProvider &session, const ConnectionConfig &config)
        : session_(session)
        , config_(config)
        , state_(ConnMgrState::UnpairedDiscovery)
        , lastHeardMs_(0)
        , stateEnteredMs_(0)
        , nextProbeMs_(0)
        , probeCount_(0)
        , connected_(false)
        , began_(false)
{
}

void ConnectionManager::begin(uint32_t nowMs)
{
        began_ = true;

        if (session_.hasPairing()) {
                // Have stored pairing — wait for coordinator to come online
                transitionTo(ConnMgrState::PairedDegraded, nowMs);
                // Boot grace period: don't probe immediately, let coordinator boot
                nextProbeMs_ = nowMs + config_.bootGracePeriodMs;
        } else {
                // No pairing — start discovery
                transitionTo(ConnMgrState::UnpairedDiscovery, nowMs);
                session_.startDiscovery();
        }
}

void ConnectionManager::loop(uint32_t nowMs)
{
        if (!began_)
                return;

        switch (state_) {
        case ConnMgrState::UnpairedDiscovery:
                handleUnpairedDiscovery(nowMs);
                break;
        case ConnMgrState::PairedConnected:
                handlePairedConnected(nowMs);
                break;
        case ConnMgrState::PairedDegraded:
                handlePairedDegraded(nowMs);
                break;
        case ConnMgrState::ReacquiringStatic:
                handleReacquiringStatic(nowMs);
                break;
        case ConnMgrState::ReacquiringDynamic:
                handleReacquiringDynamic(nowMs);
                break;
        }
}

void ConnectionManager::onHeartbeatReceived(uint32_t nowMs)
{
        if (!session_.hasPairing())
                return;

        // During broad reacquisition, ignore stale messages — only
        // onReacquisitionSuccess (verified coordinator response) is accepted
        if (state_ == ConnMgrState::ReacquiringDynamic)
                return;

        handleMessageFromCoordinator(nowMs);
}

void ConnectionManager::onMessageReceived(uint32_t nowMs)
{
        if (!session_.hasPairing())
                return;
        if (state_ == ConnMgrState::ReacquiringDynamic)
                return;

        handleMessageFromCoordinator(nowMs);
}

void ConnectionManager::onReacquisitionSuccess(uint32_t nowMs)
{
        lastHeardMs_ = nowMs;
        connected_ = true;
        session_.resetReacquisition();
        transitionTo(ConnMgrState::PairedConnected, nowMs);
}

// --- Private ---

void ConnectionManager::handleMessageFromCoordinator(uint32_t nowMs)
{
        lastHeardMs_ = nowMs;

        if (!connected_) {
                connected_ = true;
                session_.resetReacquisition();
                transitionTo(ConnMgrState::PairedConnected, nowMs);
        }
}

void ConnectionManager::transitionTo(ConnMgrState newState, uint32_t nowMs)
{
        state_ = newState;
        stateEnteredMs_ = nowMs;
        probeCount_ = 0;
}

void ConnectionManager::handleUnpairedDiscovery(uint32_t nowMs)
{
        session_.loopDiscovery(nowMs);

        if (session_.isDiscoveryComplete()) {
                connected_ = true;
                lastHeardMs_ = nowMs;
                transitionTo(ConnMgrState::PairedConnected, nowMs);
        }
}

void ConnectionManager::handlePairedConnected(uint32_t nowMs)
{
        if (lastHeardMs_ > 0 && (nowMs - lastHeardMs_) > config_.heartbeatTimeoutMs) {
                connected_ = false;
                log_warn_m("conn_mgr", "Heartbeat timeout (%lums)",
                           static_cast<unsigned long>(config_.heartbeatTimeoutMs));
                transitionTo(ConnMgrState::PairedDegraded, nowMs);
        }
}

void ConnectionManager::handlePairedDegraded(uint32_t nowMs)
{
        if (connected_) {
                transitionTo(ConnMgrState::PairedConnected, nowMs);
                return;
        }

        if ((nowMs - stateEnteredMs_) < config_.degradedGracePeriodMs) {
                return;
        }

        log_warn_m("conn_mgr", "Degraded grace expired, starting recovery");
        transitionTo(ConnMgrState::ReacquiringStatic, nowMs);
        nextProbeMs_ = nowMs;
}

void ConnectionManager::handleReacquiringStatic(uint32_t nowMs)
{
        if (connected_) {
                transitionTo(ConnMgrState::PairedConnected, nowMs);
                return;
        }
        if (nowMs < nextProbeMs_)
                return;

        session_.sendProbe();
        probeCount_++;
        nextProbeMs_ = nowMs + config_.staticProbeIntervalMs;

        if (config_.policy == ConnectionPolicy::Dynamic && probeCount_ >= config_.staticMaxProbes) {
                log_warn_m("conn_mgr", "Static probes exhausted (%d), broad reacquisition",
                           probeCount_);
                transitionTo(ConnMgrState::ReacquiringDynamic, nowMs);
                session_.startReacquisition();
                nextProbeMs_ = nowMs;
                return;
        }
}

void ConnectionManager::handleReacquiringDynamic(uint32_t nowMs)
{
        if (connected_) {
                transitionTo(ConnMgrState::PairedConnected, nowMs);
                return;
        }
        if (nowMs < nextProbeMs_)
                return;

        session_.loopReacquisition(nowMs);
        nextProbeMs_ = nowMs + config_.dynamicProbeIntervalMs;

        if (session_.isReacquisitionComplete()) {
                connected_ = true;
                lastHeardMs_ = nowMs;
                session_.resetReacquisition();
                transitionTo(ConnMgrState::PairedConnected, nowMs);
        }
}

} // namespace ungula::net::connection
