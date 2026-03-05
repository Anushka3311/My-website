function scrollToSection(){
document.getElementById("events").scrollIntoView({behavior:"smooth"});
}

const eventDate = new Date("May 07, 2026 09:00:00").getTime();

setInterval(function(){

const now = new Date().getTime();
const distance = eventDate - now;

const days = Math.floor(distance / (1000 * 60 * 60 * 24));
const hours = Math.floor((distance % (1000*60*60*24)) / (1000*60*60));
const minutes = Math.floor((distance % (1000*60*60)) / (1000*60));

document.getElementById("timer").innerHTML =
days + " Days " + hours + " Hours " + minutes + " Minutes";

},1000);