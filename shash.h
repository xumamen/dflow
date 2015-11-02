#define FALSE 0
#define TRUE 1

#define UINT8 8
#define UINT16 16
#define UINT32 32
#define UINT64 64



/* Creates a message struct with the given length at variable name */
#define MESSAGE(name) \
    MSG name; \
    name.data = NULL; \
    name.offset = 0;

/* Appropriate hton based on lenght */
#define HTON(length, value) hton##length(value)

/* uint*_t type based on length */
#define TYPE(length) uint##length##_t


/* Packs a value of the given length to a message */
#define PACK(msg, name, value, length) \
    msg.data = realloc(msg.data, msg.offset + length/8); \
    *((TYPE(length)*) (msg.data + msg.offset)) = HTON(length, value); \
    msg.offset += length/8

/* Adds a padding of the given length to a message */
#define PADDING(msg, length) \
    msg.data = realloc(msg.data, msg.offset + length); \
    memset(msg.data + msg.offset, 0, length); \
    msg.offset += length

/* Sets the ofp_header length property based on message length */
#define SET_OFP_HEADER_LENGTH(msg) \
    *((uint16_t*) msg.data + 1) =  hton16(msg.offset)


/* Prepares and sends the message */
#define SEND(name) SET_OFP_HEADER_LENGTH(name); net_send(name);


/* Fill an oxm_header */
#define OXM_HEADER(class, field, hasmask, length) \
    ((class << 16) | (field << 9) | (hasmask << 8) | (length/8))

/* Get the padding needed for some structs */
#define OFP_MATCH_OXM_PADDING(length) \
    ((length + 7)/8*8 - length)

#define OFP_ACTION_SET_FIELD_OXM_PADDING(oxm_len) \
    (((oxm_len + 4) + 7)/8*8 - (oxm_len + 4))

/* Extract fields from an oxm_header */
#define UNPACK_OXM_VENDOR(header) (header >> 16)
#define UNPACK_OXM_FIELD(header) ((header >> 9) & 0x0000007F)
#define UNPACK_OXM_HASMASK(header) ((header >> 8) & 0x00000001)
#define UNPACK_OXM_LENGTH(header) (header & 0x000000FF)

/* Rename types for a prettier code */
typedef struct ofp_header OFP_HEADER;
typedef struct ofp_header OFP_MATCH;
typedef struct ofp_flow_mod OFP_FLOW_MOD;
typedef struct ofp_instruction_actions OFP_INSTRUCTION_ACTIONS;
typedef struct ofp_action_output OFP_ACTION_OUTPUT;
typedef struct ofp_action_mpls_label OFP_ACTION_MPLS_LABEL;

/* A containter for a message with progressive packing */
typedef struct msg {
    void* data;
    uint16_t offset;
} MSG;

