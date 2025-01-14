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
 *
 * @flow strict-local
 * @format
 */

export type AccessRoleLevel = 0 | 1 | 3;

export const AccessRoles = {
  USER: 0,
  READ_ONLY_USER: 1,
  SUPERUSER: 3,
};

export const UserRoles = {
  USER: 0,
  READ_ONLY_USER: 1,
  SUPERUSER: 3,
};
