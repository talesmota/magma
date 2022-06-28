/*
 * Copyright 2022 The Magma Authors.
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
import Grid from '@material-ui/core/Grid';
import React from 'react';
import {makeStyles} from '@material-ui/styles';

import CbsdsTable from './CbsdsTable';

const useStyles = makeStyles(theme => ({
  root: {
    margin: theme.spacing(3),
    flexGrow: 1,
  },
}));

export default function Cbsds() {
  const classes = useStyles();
  return (
    <div className={classes.root}>
      <Grid container justify="space-between" spacing={3}>
        <Grid item xs={12}>
          <CbsdsTable />
        </Grid>
      </Grid>
    </div>
  );
}
