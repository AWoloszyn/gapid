
# ERR_UNKNOWN_DEVICE

Unknown device.

# ERR_FRAMEBUFFER_UNAVAILABLE

The framebuffer is not available at this point in the trace.

# ERR_NO_TEXTURE_DATA

No texture data has been associated with texture {{texture_name}} at this point in the trace.

# ERR_STATE_UNAVAILABLE

The state is not available at this point in the trace.

# ERR_VALUE_OUT_OF_BOUNDS

The value {{value}} for {{variable}} is out of bounds. Acceptable range: \[{{min}}-{{max}}\].

# ERR_SLICE_OUT_OF_BOUNDS

The slice {{from_value}}:{{to_value}} for {{from_variable}}:{{to_variable}} is out of bounds. Acceptable range: \[{{min}}-{{max}}\].

# ERR_INVALID_VALUE

Invalid value.

# ERR_INVALID_VALUE_NEGATIVE

Invalid value {{value:s64}}: The value must be positive or zero.

# ERR_INVALID_ENUM

Invalid enum {{value:u32}}.

# ERR_INVALID_OPERATION

Invalid operation.

# ERR_CONTEXT_DOES_NOT_EXIST

No context with id {{id:u64}} exists.

# ERR_NO_CONTEXT_BOUND

No context bound in thread: {{thread:u64}}

# ERR_CONTEXT_BOUND

Can not bind context with id {{id:u64}} since it is already bound on different thread.

# ERR_FIELD_DOES_NOT_EXIST

Value of type {{ty}} does not have field {{field}}.

# ERR_PARAMETER_DOES_NOT_EXIST

Command of type {{ty}} does not have parameter {{field}}.

# ERR_RESULT_DOES_NOT_EXIST

Command of type {{ty}} does not have a result value.

# ERR_MAP_KEY_DOES_NOT_EXIST

Map does not contain entry with key {{key}}.

# ERR_MESH_NOT_AVAILABLE

Mesh not available.

# ERR_MESH_HAS_NO_VERTICES

Mesh has no vertices.

# ERR_NO_PROGRAM_BOUND

No program bound.

# ERR_INCORRECT_MAP_KEY_TYPE

Incorrect map key type. Got type {{got}}, expected type {{expected}}.

# ERR_TYPE_NOT_ARRAY_INDEXABLE

Value of type {{ty}} is not array-indexable.

# ERR_TYPE_NOT_MAP_INDEXABLE

Value of type {{ty}} is not a map-indexable.

# ERR_TYPE_NOT_SLICEABLE

Value of type {{ty}} is not sliceable.

# ERR_NIL_POINTER_DEREFERENCE

The object was nil.

# ERR_UNSUPPORTED_CONVERSION

The object cannot be cast to the requested type.

# ERR_CRITICAL

Internal error: {{err}}

# ERR_TRACE_ASSERT

Internal error in trace assert: {{reason}}

# ERR_MESSAGE

{{error}}

# ERR_INTERNAL_ERROR

Internal error: {{error}}

# ERR_REPLAY_DRIVER

Error during replay: {{replayError}}

# ERR_WRONG_CONTEXT_VERSION

Required context of at least {{reqmajor:u32}}.{{reqminor:u32}}, got {{major:u32}}.{{minor:u32}}.

# WARN_UNKNOWN_CONTEXT

The context {{id:u64}} was created before tracing begun. Context state is not known.

# ERR_VALUE_NEG

{{valname}} was negative ({{value:s64}}).

# ERR_VALUE_GE_LIMIT

{{valname}} was greater than or equal to {{limitname}}. {{valname}}: {{val:s64}}, {{limitname}}: {{limit:s64}}

# ERR_NOT_A_DRAW_CALL

The requested command range does not contain any draw calls.

# TAG_ATOM_NAME

{{atom}}

# ERR_PATH_WITHOUT_CAPTURE

The request path does not contain the required capture identifier.

# NO_NEW_BUILDS_AVAILABLE

There are no new builds available.

# ERR_INVALID_MEMORY_POOL

Pool {{pool}} not found.

# ERR_FILE_CANNOT_BE_READ

The file cannot be read.

# ERR_FILE_TOO_NEW

The file was created by a more recent version of GAPID and cannot be read.

# ERR_FILE_TOO_OLD

The file was created by an old version of GAPID and cannot be read.
