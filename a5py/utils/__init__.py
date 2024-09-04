"""Utility tools that are i) not related to physics, ii) not specific to ASCOT5,
and iii) which don't fit anywhere else.
"""
import re
import numpy as np
import unyt
from typing import List, Union


Scalar = Union[int, float]
"""Type of a scalar."""

ArrayLike = Union[List[Scalar], np.ndarray, unyt.unyt_array]
"""Type of a numerical array."""

Numerical = Union[Scalar, ArrayLike]
"""Type of numerical data."""


def format2universaldate(date):
    """Convert a datetime object to a string in the format ASCOT5 uses.

    Parameters
    ----------
    date : datetime.datetime
        The datetime object to be converted.

    Returns
    -------
    formatted_date : str
        The datetime object converted to a string in the format
        "YYYY-MM-DD HH:MM:SS".
    """
    return date.strftime("%Y-%m-%d %H:%M:%S")


def validate_variables(variables, names, shape, dtype):
    """Validate the shape and dtype of multiple variables.

    This function checks that a variable can be inferred to have the expected
    format (e.g. it can be cast into the expected shape) and returns the
    variables cast exactly in the expected format.

    Parameters
    ----------
    variables : array_like or list[array_like]
        The variable(s) to validate.

        If dictionary is provided, the variables are cast in place.
    names : string or list of strings
        Names of the variables (required to generate error message).
    shape : tuple of ints or a list of tuples of ints
        The expected shape for all variables or for each variable separately.
    dtype : type
        The expected type for all variables or for each variable separately.

    Returns
    -------
    *cast_variables : array_like
        The variables cast to the expected format.

    Raises
    ------
    ValueError
        If not able to infer a variable to have the expected format.
    """
    if not isinstance(variables, list):
        variables = [variables]
        names = [names]
    if not isinstance(shape, list):
        shape = [shape] * len(variables)
    if not isinstance(dtype, list):
        dtype = [dtype] * len(variables)

    cast_variables = []
    for var, name, expshape, exptype in zip(variables, names, shape, dtype):
        try:
            varshape = var.shape
            var = np.reshape(var, expshape)
        except ValueError as e:
            raise ValueError(
                f"Argument {name} has incompatible shape {varshape}, "
                f"expected: {expshape}."
                ) from None
        vartype = var.dtype
        var_original, var = var, var.astype(exptype)
        equivalent = ( var_original == var if varshape == ()
                      else (var_original == var).all() )
        if not equivalent:
            raise ValueError(
                f"Argument {name} has incompatible type {vartype}, "
                f"expected: {exptype}."
                )
        cast_variables.append(var)
    if len(cast_variables) == 1:
        return cast_variables[0]
    return tuple(cast_variables)


def decorate(string, color=None, bold=False, underline=False):
    """Make text underlined, bold, and/or colourful.

    Parameters
    ----------
    string : str
        String to be decorated.

    Returns
    -------
    decorated : str
        The decorated string.
    """
    colors = {
        "green":"\033[32m",
        "purple":"\033[35m",
    }
    effects = {
        "reset":"\033[0m",
        "bold":"\033[01m",
        "underline":"\033[04m",
    }

    if color:
        if color not in colors:
            raise ValueError(
                f"Unknown color: {color}. "
                f"Available colors are: {colors.keys()}.")
        color = colors[color]
    else:
        color = ""

    bold = effects["bold"] if bold else ""
    underline = effects["underline"] if underline else ""
    return f"{bold}{underline}{color}{string}{effects["reset"]}"


def undecorate(string):
    """Remove decorations (ANSI escape sequences) from a string.

    Parameters
    ----------
    string : str
        A string to undecorate.

    Returns
    -------
    undecorated : str
        The undecorated string.
    """
    return re.compile(r"\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])").sub("", string)
