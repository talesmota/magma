// Code generated by go-swagger; DO NOT EDIT.

package baremetal

// This file was generated by the swagger tool.
// Editing this file might prove futile when you re-run the swagger generate command

import (
	"fmt"
	"io"

	"github.com/go-openapi/runtime"

	strfmt "github.com/go-openapi/strfmt"

	models "magma/orc8r/cloud/api/v1/go/models"
)

// PostCiNodesNodeIDReleaseReader is a Reader for the PostCiNodesNodeIDRelease structure.
type PostCiNodesNodeIDReleaseReader struct {
	formats strfmt.Registry
}

// ReadResponse reads a server response into the received o.
func (o *PostCiNodesNodeIDReleaseReader) ReadResponse(response runtime.ClientResponse, consumer runtime.Consumer) (interface{}, error) {
	switch response.Code() {
	case 204:
		result := NewPostCiNodesNodeIDReleaseNoContent()
		if err := result.readResponse(response, consumer, o.formats); err != nil {
			return nil, err
		}
		return result, nil
	default:
		result := NewPostCiNodesNodeIDReleaseDefault(response.Code())
		if err := result.readResponse(response, consumer, o.formats); err != nil {
			return nil, err
		}
		if response.Code()/100 == 2 {
			return result, nil
		}
		return nil, result
	}
}

// NewPostCiNodesNodeIDReleaseNoContent creates a PostCiNodesNodeIDReleaseNoContent with default headers values
func NewPostCiNodesNodeIDReleaseNoContent() *PostCiNodesNodeIDReleaseNoContent {
	return &PostCiNodesNodeIDReleaseNoContent{}
}

/*PostCiNodesNodeIDReleaseNoContent handles this case with default header values.

Node released successfully
*/
type PostCiNodesNodeIDReleaseNoContent struct {
}

func (o *PostCiNodesNodeIDReleaseNoContent) Error() string {
	return fmt.Sprintf("[POST /ci/nodes/{node_id}/release][%d] postCiNodesNodeIdReleaseNoContent ", 204)
}

func (o *PostCiNodesNodeIDReleaseNoContent) readResponse(response runtime.ClientResponse, consumer runtime.Consumer, formats strfmt.Registry) error {

	return nil
}

// NewPostCiNodesNodeIDReleaseDefault creates a PostCiNodesNodeIDReleaseDefault with default headers values
func NewPostCiNodesNodeIDReleaseDefault(code int) *PostCiNodesNodeIDReleaseDefault {
	return &PostCiNodesNodeIDReleaseDefault{
		_statusCode: code,
	}
}

/*PostCiNodesNodeIDReleaseDefault handles this case with default header values.

Unexpected Error
*/
type PostCiNodesNodeIDReleaseDefault struct {
	_statusCode int

	Payload *models.Error
}

// Code gets the status code for the post ci nodes node ID release default response
func (o *PostCiNodesNodeIDReleaseDefault) Code() int {
	return o._statusCode
}

func (o *PostCiNodesNodeIDReleaseDefault) Error() string {
	return fmt.Sprintf("[POST /ci/nodes/{node_id}/release][%d] PostCiNodesNodeIDRelease default  %+v", o._statusCode, o.Payload)
}

func (o *PostCiNodesNodeIDReleaseDefault) GetPayload() *models.Error {
	return o.Payload
}

func (o *PostCiNodesNodeIDReleaseDefault) readResponse(response runtime.ClientResponse, consumer runtime.Consumer, formats strfmt.Registry) error {

	o.Payload = new(models.Error)

	// response payload
	if err := consumer.Consume(response.Body(), o.Payload); err != nil && err != io.EOF {
		return err
	}

	return nil
}