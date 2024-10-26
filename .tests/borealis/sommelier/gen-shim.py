#!/usr/bin/env python3
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A C++ code generator of Wayland protocol shims."""

import os
from pathlib import Path
import sys
from typing import List, Optional
from xml.etree import ElementTree

# pylint: disable=import-error
import jinja2


def CppTypeForWaylandEventType(xml_type_string: str, interface: str) -> str:
    """Generates the type for a wayland event argument.

    Converts a wayland type to a CPP type for an event.

    Args:
        xml_type_string: The string describing the type of the wayland type
            that's present inside the wayland protocol XML.
        interface: The string describing the interface of the object.

    Returns:
        A string which contains the CPP type which will be inserted into the
        generated shim.
    """
    if xml_type_string == "new_id" or xml_type_string == "object":
        return "struct wl_resource *"
    else:
        return CppTypeForWaylandType(xml_type_string, interface)


def CppTypeForWaylandType(xml_type_string: str, interface: str) -> str:
    """Generates the type for generic wayland type.

    Converts a wayland type to a CPP type in a generalized fashion.

    Args:
        xml_type_string: The string describing the type of the wayland type
            that's present inside the wayland protocol XML.
        interface: The string describing the interface of the object.

    Returns:
        A string which contains the CPP type which will be inserted into the
        generated shim.
    """
    if xml_type_string == "int" or xml_type_string == "fd":
        return "int32_t"
    elif xml_type_string == "uint" or xml_type_string == "new_id":
        return "uint32_t"
    elif xml_type_string == "fixed":
        return "wl_fixed_t"
    elif xml_type_string == "string":
        return "const char *"
    elif xml_type_string == "object":
        return f"struct {interface}*"
    elif xml_type_string == "array":
        return "struct wl_array*"
    else:
        raise ValueError(f"Invalid Type conversion: {xml_type_string}")


def GetRequestReturnType(arguments: List[object]) -> str:
    """Gets the return type of a Wayland request.

    Args:
        arguments: A list of XML tag objects which contains the information
            about all arguments that will be provided to the request

    Returns:
        A string which contains the CPP return type of the function.
    """
    for arg in arguments:
        if arg.attrib["type"] == "new_id":
            if "interface" in arg.attrib:
                return f"struct {arg.attrib['interface']}*"
            else:
                return "void *"
    return "void"


def RequestXmlToJinjaInput(request: object) -> dict:
    """Parses a |request| element into dictionary form for use of jinja \
    template.

    Args:
        request: An XML tag object which contains the information about a
            request.

    Returns:
        A dictionary of format
        {
            "name": Method name of the request (e.g. set_anchor),
            "args": [
                {
                    "name": Name of the argument (e.g. x),
                    "type": CPP Type of the argument (e.g. int)
                }
            ],
            "ret": Return value of the request
        }
        which is used by the jinja template to generate the shim contents for a
        request.
    """
    method = {"name": request.attrib["name"], "args": [], "ret": ""}
    method["ret"] = GetRequestReturnType(request.findall("arg"))

    for arg in request.findall("arg"):
        if arg.attrib["type"] == "new_id":
            if not arg.attrib.get("interface"):
                method["args"].append(
                    {
                        "type": "const struct wl_interface *",
                        "name": "interface",
                    }
                )
                method["args"].append({"type": "uint32_t", "name": "version"})
        else:
            method["args"].append(
                {
                    "name": arg.attrib["name"],
                    "type": CppTypeForWaylandType(
                        arg.attrib["type"], arg.attrib.get("interface", "")
                    ),
                }
            )
    return method


def EventXmlToJinjaInput(event: object) -> dict:
    """Parses an |event| element into dictionary for use of jinja template.

    Args:
        event: An XML tag object which contains information about an event.

    Returns:
        A dictionary of format
        {
            "name": Name of the event (e.g. ping)
            "args": [
                {
                    "name": Name of the argument (e.g. serial)
                    "type": CPP Type of the argument (e.g. uint_32t)
                }
            ]
        }
        which is used by the jinja template to generate shim contents for an
        event.
    """
    return {
        "name": event.attrib["name"],
        "args": [
            {
                "name": arg.attrib["name"],
                "type": CppTypeForWaylandEventType(
                    arg.attrib["type"], arg.attrib.get("interface", "")
                ),
            }
            for arg in event.findall("arg")
        ],
    }


def InterfaceXmlToJinjaInput(interface: object) -> dict:
    """Creates an interface dict for XML interface input.

    Args:
        interface: An XML tag object which contains the information about an
            interface.

    Returns:
        A dictionary of format
        {
            "name": The name of the interface,
            "name_underscore": Name of the interface in snake_case,
            "methods": [<Request>] # Where request dicts are defined above.
                Naming dfference due to wayland-scanner patterns.
            "events": [<Event>] # Where event dicts are defined above.
        }
        which is used by the jinja template to generate shim contents for the
        interface.
    """
    interf = {
        "name": "".join(
            [x.capitalize() for x in interface.attrib["name"].split("_")]
        )
        + "Shim",
        "name_underscore": interface.attrib["name"],
        "methods": [
            RequestXmlToJinjaInput(x) for x in interface.findall("request")
        ],
        "events": [EventXmlToJinjaInput(x) for x in interface.findall("event")],
    }
    return interf


def GenerateShims(in_xml: str, out_directory: str) -> None:
    """Generates shims for Wayland Protocols.

    Args:
        in_xml: The location of the .xml definition of a wayland protocol.
        out_directory: The directory to output the generated shim files.
    """
    template_dir = os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "gen"
    )
    template_dir = Path(__file__).resolve().parent / "gen"
    h_data = (template_dir / "protocol-shim.h.jinja2").read_text(
        encoding="utf-8"
    )
    shim_template = jinja2.Template(h_data)

    cc_data = (template_dir / "protocol-shim.cc.jinja2").read_text(
        encoding="utf-8"
    )
    shim_impl_template = jinja2.Template(cc_data)

    mock_data = (template_dir / "mock-protocol-shim.h.jinja2").read_text(
        encoding="utf-8"
    )
    mock_template = jinja2.Template(mock_data)

    tree = ElementTree.parse(in_xml)
    root = tree.getroot()

    filename = os.path.basename(in_xml).split(".")[0]

    # Because some protocol files don't have the protocol name == file name, we
    # have to infer the name from the file name instead (gtk-shell :eyes:).
    protocol = {
        "interfaces": [
            InterfaceXmlToJinjaInput(x) for x in root.findall("interface")
        ],
        "name_hyphen": filename,
        "name_underscore": filename.replace("-", "_"),
    }

    (out_directory / (protocol["name_hyphen"] + "-shim.h")).write_text(
        shim_template.render(protocol=protocol), encoding="utf-8"
    )
    (out_directory / (protocol["name_hyphen"] + "-shim.cc")).write_text(
        shim_impl_template.render(protocol=protocol), encoding="utf-8"
    )
    (
        out_directory / ("mock-" + protocol["name_hyphen"] + "-shim.h")
    ).write_text(mock_template.render(protocol=protocol), encoding="utf-8")


def main(argv: Optional[List[str]]):
    assert len(argv) == 2
    source_xml = Path(argv[0])
    out_dir = Path(argv[1])
    GenerateShims(source_xml, out_dir)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
