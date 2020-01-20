// Copyright (c) 2004-present Facebook All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package exporter

import (
	"bytes"
	"context"
	"encoding/csv"
	"io"
	"mime/multipart"
	"strings"
	"testing"

	"github.com/facebookincubator/symphony/graph/ent/equipment"
	"github.com/facebookincubator/symphony/graph/importer"
	"github.com/stretchr/testify/require"
)

func writeModifiedLinksCSV(t *testing.T, r *csv.Reader, method method) (*bytes.Buffer, string) {
	var newLine []string
	var lines = make([][]string, 3)
	var buf bytes.Buffer
	bw := multipart.NewWriter(&buf)

	fileWriter, err := bw.CreateFormFile("file_0", "name1")
	require.Nil(t, err)
	for i := 0; ; i++ {
		line, err := r.Read()
		if err != nil {
			if err == io.EOF {
				break
			}
			require.Nil(t, err)
		}
		if i == 0 {
			lines[0] = line
		} else {
			switch method {
			case MethodAdd:
				newLine = append([]string{""}, line[1:]...)
			case MethodEdit:
				newLine = line
				if line[1] == portName1 {
					newLine[26] = "new-prop-value"
					newLine[27] = "true"
					newLine[28] = "10"
				}
			default:
				require.Fail(t, "method should be add or edit")
			}
			lines[i] = newLine
		}
	}

	for _, l := range lines {
		stringLine := strings.Join(l, ",")
		fileWriter.Write([]byte(stringLine + "\n"))
	}
	ct := bw.FormDataContentType()
	require.NoError(t, bw.Close())
	return &buf, ct
}

func TestExportAndEditLinks(t *testing.T) {
	r, err := newExporterTestResolver(t)
	require.NoError(t, err)
	log := r.exporter.log
	e := &exporter{log, linksRower{log}}
	ctx, res := prepareLinksPortsAndExport(t, r, e)
	defer res.Body.Close()
	importLinksPortsFile(t, r.client, res.Body, importer.ImportEntityLink, MethodEdit)

	locs := r.client.Location.Query().AllX(ctx)
	require.Len(t, locs, 3)
	links, err := r.Query().LinkSearch(ctx, nil, nil)
	require.NoError(t, err)
	require.Equal(t, 1, links.Count)
	for _, link := range links.Links {
		props := link.QueryProperties().AllX(ctx)
		for _, prop := range props {
			switch prop.QueryType().OnlyX(ctx).Name {
			case propNameInt:
				require.Equal(t, 10, prop.IntVal)
			case propNameBool:
				require.Equal(t, true, prop.BoolVal)
			case propNameStr:
				require.Equal(t, "new-prop-value", prop.StringVal)
			}
		}
	}
}

func TestExportAndAddLinks(t *testing.T) {
	r, err := newExporterTestResolver(t)
	require.NoError(t, err)
	log := r.exporter.log
	e := &exporter{log, linksRower{log}}
	ctx, res := prepareLinksPortsAndExport(t, r, e)
	defer res.Body.Close()

	locs := r.client.Location.Query().AllX(ctx)
	require.Len(t, locs, 3)
	// Deleting link and of side's equipment to verify it creates it on import
	deleteLinkAndEquipmentForReImport(ctx, t, r)

	equips := r.client.Equipment.Query().AllX(ctx)
	require.Len(t, equips, 1)
	importLinksPortsFile(t, r.client, res.Body, importer.ImportEntityLink, MethodAdd)

	links, err := r.Query().LinkSearch(ctx, nil, nil)
	require.NoError(t, err)
	require.Equal(t, 1, links.Count)
	for _, link := range links.Links {
		props := link.QueryProperties().AllX(ctx)
		for _, prop := range props {
			switch prop.QueryType().OnlyX(ctx).Name {
			case propNameInt:
				require.Equal(t, 100, prop.IntVal)
			case propNameBool:
				require.Equal(t, false, prop.BoolVal)
			case propNameStr:
				require.Equal(t, "t1", prop.StringVal)
			}
		}
	}
}

func deleteLinkAndEquipmentForReImport(ctx context.Context, t *testing.T, r *TestExporterResolver) {
	l := r.client.Link.Query().OnlyX(ctx)
	equipToDelete := l.QueryPorts().QueryParent().Where(equipment.Name(currEquip)).OnlyX(ctx)
	_, err := r.Mutation().RemoveLink(ctx, l.ID, nil)
	require.NoError(t, err)
	_, err = r.Mutation().RemoveEquipment(ctx, equipToDelete.ID, nil)
	require.NoError(t, err)
}
