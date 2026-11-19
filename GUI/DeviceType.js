.pragma library

var Options = ["ONYX Pro", "ONYX-A Pro"]
var Default = "ONYX-A Pro"

function normalized(value) {
    return String(value === undefined || value === null ? "" : value).trim().toUpperCase()
}

function isOnyxM(value) {
    var n = normalized(value)
    return n === "ONYX-M" || n === "ONYX PRO" || n === "ONYXPRO"
}

function isOnyxAm(value) {
    var n = normalized(value)
    return n === "ONYX-AM" || n === "ONYX-A PRO" || n === "ONYX A PRO" || n === "ONYXAPRO"
}

function displayName(value) {
    if (!normalized(value))
        return Default
    return isOnyxM(value) ? "ONYX Pro" : "ONYX-A Pro"
}

function canonicalId(value) {
    return isOnyxM(value) ? "ONYX-M" : "ONYX-AM"
}
