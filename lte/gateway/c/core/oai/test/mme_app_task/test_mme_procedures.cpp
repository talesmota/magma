/**
 * Copyright 2020 The Magma Authors.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <chrono>
#include <gtest/gtest.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <stdio.h>

#include "feg/protos/s6a_proxy.pb.h"
#include "../mock_tasks/mock_tasks.h"
#include "mme_app_state_manager.h"
#include "mme_app_ip_imsi.h"
#include "proto_msg_to_itti_msg.h"
#include "mme_app_test_util.h"

extern "C" {
#include "common_types.h"
#include "mme_config.h"
#include "mme_app_extern.h"
#include "mme_app_state.h"
}

using ::testing::_;
using ::testing::Return;

extern bool mme_hss_associated;
extern bool mme_sctp_bounded;

namespace magma {
namespace lte {

ACTION_P(ReturnFromAsyncTask, cv) {
  cv->notify_all();
}

task_zmq_ctx_t task_zmq_ctx_main;

static int handle_message(zloop_t* loop, zsock_t* reader, void* arg) {
  MessageDef* received_message_p = receive_msg(reader);

  switch (ITTI_MSG_ID(received_message_p)) {
    default: { } break; }

  itti_free_msg_content(received_message_p);
  free(received_message_p);
  return 0;
}

class MmeAppProcedureTest : public ::testing::Test {
  virtual void SetUp() {
    mme_hss_associated = false;
    mme_sctp_bounded   = false;
    s1ap_handler       = std::make_shared<MockS1apHandler>();
    s6a_handler        = std::make_shared<MockS6aHandler>();
    spgw_handler       = std::make_shared<MockSpgwHandler>();
    service303_handler = std::make_shared<MockService303Handler>();
    itti_init(
        TASK_MAX, THREAD_MAX, MESSAGES_ID_MAX, tasks_info, messages_info, NULL,
        NULL);

    // initialize mme config
    mme_config_init(&mme_config);
    nas_config_timer_reinit(&mme_config.nas_config, MME_APP_TIMER_TO_MSEC);
    create_partial_lists(&mme_config);
    mme_config.use_stateless                              = true;
    mme_config.nas_config.prefered_integrity_algorithm[0] = EIA2_128_ALG_ID;

    task_id_t task_id_list[10] = {
        TASK_MME_APP,    TASK_HA,  TASK_S1AP,   TASK_S6A,      TASK_S11,
        TASK_SERVICE303, TASK_SGS, TASK_SGW_S8, TASK_SPGW_APP, TASK_SMS_ORC8R};
    init_task_context(
        TASK_MAIN, task_id_list, 10, handle_message, &task_zmq_ctx_main);

    std::thread task_ha(start_mock_ha_task);
    std::thread task_s1ap(start_mock_s1ap_task, s1ap_handler);
    std::thread task_s6a(start_mock_s6a_task, s6a_handler);
    std::thread task_s11(start_mock_s11_task);
    std::thread task_service303(start_mock_service303_task, service303_handler);
    std::thread task_sgs(start_mock_sgs_task);
    std::thread task_sgw_s8(start_mock_sgw_s8_task);
    std::thread task_sms_orc8r(start_mock_sms_orc8r_task);
    std::thread task_spgw(start_mock_spgw_task, spgw_handler);
    task_ha.detach();
    task_s1ap.detach();
    task_s6a.detach();
    task_s11.detach();
    task_service303.detach();
    task_sgs.detach();
    task_sgw_s8.detach();
    task_sms_orc8r.detach();
    task_spgw.detach();

    mme_app_init(&mme_config);
    // Fake initialize sctp server.
    // We can then send activate messages in each test
    // whenever we need to read mme_app state.
    send_sctp_mme_server_initialized();
  }

  virtual void TearDown() {
    send_terminate_message_fatal(&task_zmq_ctx_main);
    destroy_task_context(&task_zmq_ctx_main);
    itti_free_desc_threads();
    // Sleep to ensure that messages are received and contexts are released
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }

 protected:
  std::shared_ptr<MockS1apHandler> s1ap_handler;
  std::shared_ptr<MockS6aHandler> s6a_handler;
  std::shared_ptr<MockSpgwHandler> spgw_handler;
  std::shared_ptr<MockService303Handler> service303_handler;
  const uint8_t nas_msg_imsi_attach_req[31] = {
      0x07, 0x41, 0x71, 0x08, 0x09, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00,
      0x10, 0x02, 0xe0, 0xe0, 0x00, 0x04, 0x02, 0x01, 0xd0, 0x11, 0x40,
      0x08, 0x04, 0x02, 0x60, 0x04, 0x00, 0x02, 0x1c, 0x00};
  const uint8_t nas_msg_guti_attach_req[34] = {
      0x07, 0x41, 0x71, 0x0b, 0xf6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x02, 0xe0, 0xe0, 0x00, 0x04, 0x02, 0x01, 0xd0, 0x11,
      0x40, 0x08, 0x04, 0x02, 0x60, 0x04, 0x00, 0x02, 0x1c, 0x00};
  const uint8_t nas_msg_auth_resp[19] = {
      0x07, 0x53, 0x10, 0x66, 0xff, 0x47, 0x2d, 0xd4, 0x93, 0xf1,
      0x5a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  const uint8_t nas_msg_smc_resp[19] = {
      0x47, 0xc0, 0xb5, 0x35, 0x6b, 0x00, 0x07, 0x5e, 0x23, 0x09,
      0x33, 0x08, 0x45, 0x86, 0x34, 0x12, 0x31, 0x71, 0xf2};
  const uint8_t nas_msg_ident_resp[11]  = {0x07, 0x56, 0x08, 0x09, 0x10, 0x10,
                                          0x00, 0x00, 0x00, 0x00, 0x10};
  const uint8_t nas_msg_attach_comp[13] = {0x27, 0xb6, 0x28, 0x5a, 0x49,
                                           0x01, 0x07, 0x43, 0x00, 0x03,
                                           0x52, 0x00, 0xc2};
  const uint8_t nas_msg_detach_req[21]  = {
      0x27, 0x8f, 0xf4, 0x06, 0xe5, 0x02, 0x07, 0x45, 0x09, 0x0b, 0xf6,
      0x00, 0xf1, 0x10, 0x00, 0x01, 0x01, 0x46, 0x93, 0xe8, 0xb8};
  std::string imsi = "001010000000001";
  plmn_t plmn      = {.mcc_digit2 = 0,
                 .mcc_digit1 = 0,
                 .mnc_digit3 = 0x0f,
                 .mcc_digit3 = 1,
                 .mnc_digit2 = 1,
                 .mnc_digit1 = 0};
};

TEST_F(MmeAppProcedureTest, TestInitialUeMessageFaultyNasMsg) {
  mme_app_desc_t* mme_state_p =
      magma::lte::MmeNasStateManager::getInstance().get_state(false);
  std::condition_variable cv;
  std::mutex mx;
  std::unique_lock<std::mutex> lock(mx);

  MME_APP_EXPECT_CALLS(1, 0, 0, 0, 0, 0, 0, 0, 1);

  // Construction and sending Initial Attach Request to mme_app mimicing S1AP
  // The following buffer just includes an attach request
  uint8_t nas_msg_faulty[29] = {0x72, 0x08, 0x09, 0x10, 0x10, 0x00, 0x00, 0x00,
                                0x00, 0x10, 0x02, 0xe0, 0xe0, 0x00, 0x04, 0x02,
                                0x01, 0xd0, 0x11, 0x40, 0x08, 0x04, 0x02, 0x60,
                                0x04, 0x00, 0x02, 0x1c, 0x00};
  send_mme_app_initial_ue_msg(nas_msg_faulty, sizeof(nas_msg_faulty), plmn);

  // Wait for DL NAS Transport for once
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));

  // Check MME state: at this point, MME should be sending
  // EMM_STATUS NAS message and holding onto the UE context
  send_activate_message_to_mme_app();
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  EXPECT_EQ(mme_state_p->nb_ue_attached, 0);
  EXPECT_EQ(mme_state_p->nb_ue_connected, 1);
  EXPECT_EQ(mme_state_p->nb_default_eps_bearers, 0);
  EXPECT_EQ(mme_state_p->nb_ue_idle, 0);

  // Sleep to ensure that messages are received and contexts are released
  std::this_thread::sleep_for(std::chrono::milliseconds(END_OF_TEST_SLEEP_MS));
}

TEST_F(MmeAppProcedureTest, TestImsiAttachEpsOnlyDetach) {
  mme_app_desc_t* mme_state_p =
      magma::lte::MmeNasStateManager::getInstance().get_state(false);
  std::condition_variable cv;
  std::mutex mx;
  std::unique_lock<std::mutex> lock(mx);

  MME_APP_EXPECT_CALLS(3, 1, 1, 1, 1, 1, 1, 1, 2);

  // Construction and sending Initial Attach Request to mme_app mimicing S1AP
  send_mme_app_initial_ue_msg(
      nas_msg_imsi_attach_req, sizeof(nas_msg_imsi_attach_req), plmn);

  // Sending AIA to mme_app mimicing successful S6A response for AIR
  send_authentication_info_resp(imsi, true);

  // Wait for DL NAS Transport for once
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  // Constructing and sending Authentication Response to mme_app mimicing S1AP
  send_mme_app_uplink_data_ind(
      nas_msg_auth_resp, sizeof(nas_msg_auth_resp), plmn);

  // Wait for DL NAS Transport for once
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  // Constructing and sending SMC Response to mme_app mimicing S1AP
  send_mme_app_uplink_data_ind(
      nas_msg_smc_resp, sizeof(nas_msg_smc_resp), plmn);

  // Sending ULA to mme_app mimicing successful S6A response for ULR
  send_s6a_ula(imsi, true);

  // Constructing and sending Create Session Response to mme_app mimicing SPGW
  send_create_session_resp();

  // Constructing and sending ICS Response to mme_app mimicing S1AP
  send_ics_response();

  // Constructing UE Capability Indication message to mme_app
  // mimicing S1AP with dummy radio capabilities
  send_ue_capabilities_ind();

  // Constructing and sending Attach Complete to mme_app
  // mimicing S1AP
  send_mme_app_uplink_data_ind(
      nas_msg_attach_comp, sizeof(nas_msg_attach_comp), plmn);

  // Check MME state after attach complete
  send_activate_message_to_mme_app();
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  EXPECT_EQ(mme_state_p->nb_ue_attached, 1);
  EXPECT_EQ(mme_state_p->nb_ue_connected, 1);
  EXPECT_EQ(mme_state_p->nb_default_eps_bearers, 1);
  EXPECT_EQ(mme_state_p->nb_ue_idle, 0);

  // Constructing and sending Detach Request to mme_app
  // mimicing S1AP
  send_mme_app_uplink_data_ind(
      nas_msg_detach_req, sizeof(nas_msg_detach_req), plmn);

  // Constructing and sending Delete Session Response to mme_app
  // mimicing SPGW task
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  send_delete_session_resp();

  // Wait for DL NAS Transport for once
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));

  // Constructing and sending CONTEXT RELEASE COMPLETE to mme_app
  // mimicing S1AP task
  send_ue_ctx_release_complete();

  // Check MME state after detach complete
  send_activate_message_to_mme_app();
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  EXPECT_EQ(mme_state_p->nb_ue_attached, 0);
  EXPECT_EQ(mme_state_p->nb_ue_connected, 0);
  EXPECT_EQ(mme_state_p->nb_default_eps_bearers, 0);
  EXPECT_EQ(mme_state_p->nb_ue_idle, 0);

  // Sleep to ensure that messages are received and contexts are released
  std::this_thread::sleep_for(std::chrono::milliseconds(END_OF_TEST_SLEEP_MS));
}

TEST_F(MmeAppProcedureTest, TestGutiAttachEpsOnlyDetach) {
  mme_app_desc_t* mme_state_p =
      magma::lte::MmeNasStateManager::getInstance().get_state(false);
  std::condition_variable cv;
  std::mutex mx;
  std::unique_lock<std::mutex> lock(mx);

  MME_APP_EXPECT_CALLS(4, 1, 1, 1, 1, 1, 1, 1, 2);

  // Construction and sending Initial Attach Request to mme_app mimicing S1AP
  send_mme_app_initial_ue_msg(
      nas_msg_guti_attach_req, sizeof(nas_msg_guti_attach_req), plmn);

  // Wait for DL NAS Transport for once
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  // Constructing and sending Identity Response to mme_app mimicing S1AP
  send_mme_app_uplink_data_ind(
      nas_msg_ident_resp, sizeof(nas_msg_ident_resp), plmn);

  // Sending AIA to mme_app mimicing successful S6A response for AIR
  send_authentication_info_resp(imsi, true);

  // Wait for DL NAS Transport for once
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  // Constructing and sending Authentication Response to mme_app mimicing S1AP
  send_mme_app_uplink_data_ind(
      nas_msg_auth_resp, sizeof(nas_msg_auth_resp), plmn);

  // Wait for DL NAS Transport for once
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  // Constructing and sending SMC Response to mme_app mimicing S1AP
  send_mme_app_uplink_data_ind(
      nas_msg_smc_resp, sizeof(nas_msg_smc_resp), plmn);

  // Sending ULA to mme_app mimicing successful S6A response for ULR
  send_s6a_ula(imsi, true);

  // Constructing and sending Create Session Response to mme_app mimicing SPGW
  send_create_session_resp();

  // Constructing and sending ICS Response to mme_app mimicing S1AP
  send_ics_response();

  // Constructing UE Capability Indication message to mme_app
  // mimicing S1AP with dummy radio capabilities
  send_ue_capabilities_ind();

  // Constructing and sending Attach Complete to mme_app
  // mimicing S1AP
  send_mme_app_uplink_data_ind(
      nas_msg_attach_comp, sizeof(nas_msg_attach_comp), plmn);

  // Check MME state after attach complete
  send_activate_message_to_mme_app();
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  EXPECT_EQ(mme_state_p->nb_ue_attached, 1);
  EXPECT_EQ(mme_state_p->nb_ue_connected, 1);
  EXPECT_EQ(mme_state_p->nb_default_eps_bearers, 1);
  EXPECT_EQ(mme_state_p->nb_ue_idle, 0);

  // Constructing and sending Detach Request to mme_app
  // mimicing S1AP
  send_mme_app_uplink_data_ind(
      nas_msg_detach_req, sizeof(nas_msg_detach_req), plmn);

  // Constructing and sending Delete Session Response to mme_app
  // mimicing SPGW task
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  send_delete_session_resp();

  // Wait for DL NAS Transport for once
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));

  // Constructing and sending CONTEXT RELEASE COMPLETE to mme_app
  // mimicing S1AP task
  send_ue_ctx_release_complete();

  // Check MME state after detach complete
  send_activate_message_to_mme_app();
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  EXPECT_EQ(mme_state_p->nb_ue_attached, 0);
  EXPECT_EQ(mme_state_p->nb_ue_connected, 0);
  EXPECT_EQ(mme_state_p->nb_default_eps_bearers, 0);
  EXPECT_EQ(mme_state_p->nb_ue_idle, 0);

  // Sleep to ensure that messages are received and contexts are released
  std::this_thread::sleep_for(std::chrono::milliseconds(END_OF_TEST_SLEEP_MS));
}

TEST_F(MmeAppProcedureTest, TestImsiAttachEpsOnlyAirFailure) {
  mme_app_desc_t* mme_state_p =
      magma::lte::MmeNasStateManager::getInstance().get_state(false);
  std::condition_variable cv;
  std::mutex mx;
  std::unique_lock<std::mutex> lock(mx);

  MME_APP_EXPECT_CALLS(1, 0, 1, 1, 0, 0, 0, 0, 2);

  // Construction and sending Initial Attach Request to mme_app mimicing S1AP
  send_mme_app_initial_ue_msg(
      nas_msg_imsi_attach_req, sizeof(nas_msg_imsi_attach_req), plmn);

  // Sending AIA to mme_app mimicing negative S6A response for AIR
  send_authentication_info_resp(imsi, false);

  // Wait for DL NAS Transport for once
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));

  // Check MME state: at this point, MME should be sending attach reject
  // as well as context release request
  send_activate_message_to_mme_app();
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  EXPECT_EQ(mme_state_p->nb_ue_attached, 0);
  EXPECT_EQ(mme_state_p->nb_ue_connected, 1);
  EXPECT_EQ(mme_state_p->nb_default_eps_bearers, 0);
  EXPECT_EQ(mme_state_p->nb_ue_idle, 0);

  // Constructing and sending CONTEXT RELEASE COMPLETE to mme_app
  // mimicing S1AP task
  send_ue_ctx_release_complete();

  // Check if the context is properly released
  send_activate_message_to_mme_app();
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  EXPECT_EQ(mme_state_p->nb_ue_attached, 0);
  EXPECT_EQ(mme_state_p->nb_ue_connected, 0);
  EXPECT_EQ(mme_state_p->nb_default_eps_bearers, 0);
  EXPECT_EQ(mme_state_p->nb_ue_idle, 0);

  // Sleep to ensure that messages are received and contexts are released
  std::this_thread::sleep_for(std::chrono::milliseconds(END_OF_TEST_SLEEP_MS));
}

TEST_F(MmeAppProcedureTest, TestImsiAttachEpsOnlyAirTimeout) {
  mme_app_desc_t* mme_state_p =
      magma::lte::MmeNasStateManager::getInstance().get_state(false);
  std::condition_variable cv;
  std::mutex mx;
  std::unique_lock<std::mutex> lock(mx);

  MME_APP_EXPECT_CALLS(1, 0, 1, 1, 0, 0, 0, 0, 2);

  // Construction and sending Initial Attach Request to mme_app mimicing S1AP
  send_mme_app_initial_ue_msg(
      nas_msg_imsi_attach_req, sizeof(nas_msg_imsi_attach_req), plmn);

  // Wait for DL NAS Transport for once
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));

  // Check MME state: at this point, MME should be sending attach reject
  // as well as context release request
  send_activate_message_to_mme_app();
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  EXPECT_EQ(mme_state_p->nb_ue_attached, 0);
  EXPECT_EQ(mme_state_p->nb_ue_connected, 1);
  EXPECT_EQ(mme_state_p->nb_default_eps_bearers, 0);
  EXPECT_EQ(mme_state_p->nb_ue_idle, 0);

  // Constructing and sending CONTEXT RELEASE COMPLETE to mme_app
  // mimicing S1AP task
  send_ue_ctx_release_complete();

  // Check if the context is properly released
  send_activate_message_to_mme_app();
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  EXPECT_EQ(mme_state_p->nb_ue_attached, 0);
  EXPECT_EQ(mme_state_p->nb_ue_connected, 0);
  EXPECT_EQ(mme_state_p->nb_default_eps_bearers, 0);
  EXPECT_EQ(mme_state_p->nb_ue_idle, 0);

  // Sleep to ensure that messages are received and contexts are released
  std::this_thread::sleep_for(std::chrono::milliseconds(END_OF_TEST_SLEEP_MS));
}

TEST_F(MmeAppProcedureTest, TestImsiAttachEpsOnlyUlaFailure) {
  mme_app_desc_t* mme_state_p =
      magma::lte::MmeNasStateManager::getInstance().get_state(false);
  std::condition_variable cv;
  std::mutex mx;
  std::unique_lock<std::mutex> lock(mx);

  MME_APP_EXPECT_CALLS(3, 0, 1, 1, 1, 0, 0, 0, 2);

  // Construction and sending Initial Attach Request to mme_app mimicing S1AP
  send_mme_app_initial_ue_msg(
      nas_msg_imsi_attach_req, sizeof(nas_msg_imsi_attach_req), plmn);

  // Sending AIA to mme_app mimicing successful S6A response for AIR
  send_authentication_info_resp(imsi, true);

  // Wait for DL NAS Transport for once
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  // Constructing and sending Authentication Response to mme_app mimicing S1AP
  send_mme_app_uplink_data_ind(
      nas_msg_auth_resp, sizeof(nas_msg_auth_resp), plmn);

  // Wait for DL NAS Transport for once
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  // Constructing and sending SMC Response to mme_app mimicing S1AP
  send_mme_app_uplink_data_ind(
      nas_msg_smc_resp, sizeof(nas_msg_smc_resp), plmn);

  // Sending ULA to mme_app mimicing negative S6A response for ULR
  send_s6a_ula(imsi, false);

  // Wait for DL NAS Transport for once
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));

  // Check MME state: at this point, MME should be sending attach reject
  // as well as context release request
  send_activate_message_to_mme_app();
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  EXPECT_EQ(mme_state_p->nb_ue_attached, 0);
  EXPECT_EQ(mme_state_p->nb_ue_connected, 1);
  EXPECT_EQ(mme_state_p->nb_default_eps_bearers, 0);
  EXPECT_EQ(mme_state_p->nb_ue_idle, 0);

  // Constructing and sending CONTEXT RELEASE COMPLETE to mme_app
  // mimicing S1AP task
  send_ue_ctx_release_complete();

  // Check if the context is properly released
  send_activate_message_to_mme_app();
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  EXPECT_EQ(mme_state_p->nb_ue_attached, 0);
  EXPECT_EQ(mme_state_p->nb_ue_connected, 0);
  EXPECT_EQ(mme_state_p->nb_default_eps_bearers, 0);
  EXPECT_EQ(mme_state_p->nb_ue_idle, 0);

  // Sleep to ensure that messages are received and contexts are released
  std::this_thread::sleep_for(std::chrono::milliseconds(END_OF_TEST_SLEEP_MS));
}

TEST_F(MmeAppProcedureTest, TestImsiAttachExpiredNasTimers) {
  mme_app_desc_t* mme_state_p =
      magma::lte::MmeNasStateManager::getInstance().get_state(false);
  std::condition_variable cv;
  std::mutex mx;
  std::unique_lock<std::mutex> lock(mx);

  MME_APP_EXPECT_CALLS(15, 1, 1, 1, 1, 1, 1, 1, 2);

  // Construction and sending Initial Attach Request to mme_app mimicing S1AP
  send_mme_app_initial_ue_msg(
      nas_msg_imsi_attach_req, sizeof(nas_msg_imsi_attach_req), plmn);

  // Sending AIA to mme_app mimicing successful S6A response for AIR
  send_authentication_info_resp(imsi, true);

  // Wait for DL NAS Transport up to retransmission limit
  for (int i = 0; i < NAS_RETX_LIMIT; ++i) {
    cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  }
  // Constructing and sending Authentication Response to mme_app mimicing S1AP
  send_mme_app_uplink_data_ind(
      nas_msg_auth_resp, sizeof(nas_msg_auth_resp), plmn);

  // Wait for DL NAS Transport up to retransmission limit
  for (int i = 0; i < NAS_RETX_LIMIT; ++i) {
    cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  }
  // Constructing and sending SMC Response to mme_app mimicing S1AP
  send_mme_app_uplink_data_ind(
      nas_msg_smc_resp, sizeof(nas_msg_smc_resp), plmn);

  // Sending ULA to mme_app mimicing successful S6A response for ULR
  send_s6a_ula(imsi, true);

  // Constructing and sending Create Session Response to mme_app mimicing SPGW
  send_create_session_resp();

  // Constructing and sending ICS Response to mme_app mimicing S1AP
  send_ics_response();

  // Constructing UE Capability Indication message to mme_app
  // mimicing S1AP with dummy radio capabilities
  send_ue_capabilities_ind();

  // Wait for DL NAS Transport up to retransmission limit.
  // The first Attach Accept was piggybacked on ICS Request.
  for (int i = 1; i < NAS_RETX_LIMIT; ++i) {
    cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  }
  // Constructing and sending Attach Complete to mme_app
  // mimicing S1AP
  send_mme_app_uplink_data_ind(
      nas_msg_attach_comp, sizeof(nas_msg_attach_comp), plmn);

  // Check MME state after attach complete
  send_activate_message_to_mme_app();
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  EXPECT_EQ(mme_state_p->nb_ue_attached, 1);
  EXPECT_EQ(mme_state_p->nb_ue_connected, 1);
  EXPECT_EQ(mme_state_p->nb_default_eps_bearers, 1);
  EXPECT_EQ(mme_state_p->nb_ue_idle, 0);

  // Constructing and sending Detach Request to mme_app
  // mimicing S1AP
  send_mme_app_uplink_data_ind(
      nas_msg_detach_req, sizeof(nas_msg_detach_req), plmn);

  // Constructing and sending Delete Session Response to mme_app
  // mimicing SPGW task
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  send_delete_session_resp();

  // Wait for DL NAS Transport for once
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));

  // Constructing and sending CONTEXT RELEASE COMPLETE to mme_app
  // mimicing S1AP task
  send_ue_ctx_release_complete();

  // Check MME state after detach complete
  send_activate_message_to_mme_app();
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  EXPECT_EQ(mme_state_p->nb_ue_attached, 0);
  EXPECT_EQ(mme_state_p->nb_ue_connected, 0);
  EXPECT_EQ(mme_state_p->nb_default_eps_bearers, 0);
  EXPECT_EQ(mme_state_p->nb_ue_idle, 0);

  // Sleep to ensure that messages are received and contexts are released
  std::this_thread::sleep_for(std::chrono::milliseconds(END_OF_TEST_SLEEP_MS));
}

TEST_F(MmeAppProcedureTest, TestGutiAttachExpiredIdentity) {
  mme_app_desc_t* mme_state_p =
      magma::lte::MmeNasStateManager::getInstance().get_state(false);
  std::condition_variable cv;
  std::mutex mx;
  std::unique_lock<std::mutex> lock(mx);

  MME_APP_EXPECT_CALLS(8, 1, 1, 1, 1, 1, 1, 1, 2);

  // Construction and sending Initial Attach Request to mme_app mimicing S1AP
  send_mme_app_initial_ue_msg(
      nas_msg_guti_attach_req, sizeof(nas_msg_guti_attach_req), plmn);

  // Wait for DL NAS Transport up to retransmission limit
  for (int i = 0; i < NAS_RETX_LIMIT; ++i) {
    cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  }
  // Constructing and sending Identity Response to mme_app mimicing S1AP
  send_mme_app_uplink_data_ind(
      nas_msg_ident_resp, sizeof(nas_msg_ident_resp), plmn);

  // Sending AIA to mme_app mimicing successful S6A response for AIR
  send_authentication_info_resp(imsi, true);

  // Wait for DL NAS Transport for once
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  // Constructing and sending Authentication Response to mme_app mimicing S1AP
  send_mme_app_uplink_data_ind(
      nas_msg_auth_resp, sizeof(nas_msg_auth_resp), plmn);

  // Wait for DL NAS Transport for once
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  // Constructing and sending SMC Response to mme_app mimicing S1AP
  send_mme_app_uplink_data_ind(
      nas_msg_smc_resp, sizeof(nas_msg_smc_resp), plmn);

  // Sending ULA to mme_app mimicing successful S6A response for ULR
  send_s6a_ula(imsi, true);

  // Constructing and sending Create Session Response to mme_app mimicing SPGW
  send_create_session_resp();

  // Constructing and sending ICS Response to mme_app mimicing S1AP
  send_ics_response();

  // Constructing UE Capability Indication message to mme_app
  // mimicing S1AP with dummy radio capabilities
  send_ue_capabilities_ind();

  // Constructing and sending Attach Complete to mme_app
  // mimicing S1AP
  send_mme_app_uplink_data_ind(
      nas_msg_attach_comp, sizeof(nas_msg_attach_comp), plmn);

  // Check MME state after attach complete
  send_activate_message_to_mme_app();
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  EXPECT_EQ(mme_state_p->nb_ue_attached, 1);
  EXPECT_EQ(mme_state_p->nb_ue_connected, 1);
  EXPECT_EQ(mme_state_p->nb_default_eps_bearers, 1);
  EXPECT_EQ(mme_state_p->nb_ue_idle, 0);

  // Constructing and sending Detach Request to mme_app
  // mimicing S1AP
  send_mme_app_uplink_data_ind(
      nas_msg_detach_req, sizeof(nas_msg_detach_req), plmn);

  // Constructing and sending Delete Session Response to mme_app
  // mimicing SPGW task
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  send_delete_session_resp();

  // Constructing and sending CONTEXT RELEASE COMPLETE to mme_app
  // mimicing S1AP task
  send_ue_ctx_release_complete();

  // Wait for DL NAS Transport for once
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));

  // Check MME state after detach complete
  send_activate_message_to_mme_app();
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  EXPECT_EQ(mme_state_p->nb_ue_attached, 0);
  EXPECT_EQ(mme_state_p->nb_ue_connected, 0);
  EXPECT_EQ(mme_state_p->nb_default_eps_bearers, 0);
  EXPECT_EQ(mme_state_p->nb_ue_idle, 0);

  // Sleep to ensure that messages are received and contexts are released
  std::this_thread::sleep_for(std::chrono::milliseconds(END_OF_TEST_SLEEP_MS));
}

TEST_F(MmeAppProcedureTest, TestIcsRequestTimeout) {
  mme_app_desc_t* mme_state_p =
      magma::lte::MmeNasStateManager::getInstance().get_state(false);
  std::condition_variable cv;
  std::mutex mx;
  std::unique_lock<std::mutex> lock(mx);

  MME_APP_EXPECT_CALLS(2, 1, 1, 1, 1, 1, 1, 1, 1);

  // Construction and sending Initial Attach Request to mme_app mimicing S1AP
  send_mme_app_initial_ue_msg(
      nas_msg_imsi_attach_req, sizeof(nas_msg_imsi_attach_req), plmn);

  // Sending AIA to mme_app mimicing successful S6A response for AIR
  send_authentication_info_resp(imsi, true);

  // Wait for DL NAS Transport for once
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  // Constructing and sending Authentication Response to mme_app mimicing S1AP
  send_mme_app_uplink_data_ind(
      nas_msg_auth_resp, sizeof(nas_msg_auth_resp), plmn);

  // Wait for DL NAS Transport for once
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  // Constructing and sending SMC Response to mme_app mimicing S1AP
  send_mme_app_uplink_data_ind(
      nas_msg_smc_resp, sizeof(nas_msg_smc_resp), plmn);

  // Sending ULA to mme_app mimicing successful S6A response for ULR
  send_s6a_ula(imsi, true);

  // Constructing and sending Create Session Response to mme_app mimicing SPGW
  send_create_session_resp();

  // Wait for ICS Request timeout
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));

  // Constructing and sending Delete Session Response to mme_app
  // mimicing SPGW task
  send_delete_session_resp();

  // Constructing and sending CONTEXT RELEASE COMPLETE to mme_app
  // mimicing S1AP task
  send_ue_ctx_release_complete();

  // Check MME state after delete session processing
  send_activate_message_to_mme_app();
  cv.wait_for(lock, std::chrono::milliseconds(STATE_MAX_WAIT_MS));
  EXPECT_EQ(mme_state_p->nb_ue_attached, 0);
  EXPECT_EQ(mme_state_p->nb_ue_connected, 0);
  EXPECT_EQ(mme_state_p->nb_default_eps_bearers, 0);
  EXPECT_EQ(mme_state_p->nb_ue_idle, 0);

  // Sleep to ensure that messages are received and contexts are released
  std::this_thread::sleep_for(std::chrono::milliseconds(END_OF_TEST_SLEEP_MS));
}

}  // namespace lte
}  // namespace magma
