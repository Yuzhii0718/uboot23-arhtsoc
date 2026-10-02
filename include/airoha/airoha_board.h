#define ID1_TYPE                (1)
#define ID1_LEN                 (1)
#define ID1_REMARK_LSB          (15)
#define	ID1_LSB                 (16)
#define REMARKD_ID1_LSB         (20)
#define ID2_REMARK_LSB          (24)
#define ID2_LSB                 (25)
#define REMARKD_ID2_LSB         (28)

#define ID2_TYPE                (2)
#define ID2_LEN                 (2)
#define ID1_REMARK_MASK         (0x1)
#define	ID1_MASK                (0xF)
#define REMARKD_ID1_MASK        (0xF)
#define ID2_REMARK_MASK         (0x1)
#define ID2_MASK                (0x7)
#define REMARKD_ID2_MASK        (0x7)

#define BOOTARGS_RFB_ID1_STR    ("rfb_id1")
#define BOOTARGS_RFB_ID2_STR    ("rfb_id2")
#define BOOTARGS_PON			("serdes_pon")
#define BOOTARGS_ETH			("serdes_ethernet")
#define BOOTARGS_WIFI1			("serdes_wifi1")
#define BOOTARGS_WIFI2			("serdes_wifi2")
#define BOOTARGS_USB1			("serdes_usb1")
#define BOOTARGS_USB2			("serdes_usb2")
#define NUM_SERDES_ARGS			(5)

typedef struct RFB_ID {
	uint8_t id1;
	uint8_t id2;
} RFB_ID_t;