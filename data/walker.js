 //    WALKER
 function Walker() {
  
    // Start Walker in center with 0 velocity
    this.pos = createVector(width / 2, height / 8)
    this.vel = createVector(0, 0)
    this.acc = createVector(0, 0)
    this.color = color(0,0,0) // 
    
    // Sets the color used in render()
    this.setColor = function(newColor) {
        this.color = newColor
    }
    // Loop - Display Walker on canvas  
    this.render = function() {
        stroke(this.color)
        fill(this.color)
        rect(this.pos.x, this.pos.y, 3, 3, 20);
    }
    
    // Loop - Actualize Walker position chasing a random ghost position.
    this.update = function() 
    {
        // Vector at random OnScreen location
        var scrpos = createVector(floor(random(0,width)), floor(random(0, height)))
        // Vector pointing from Walker to OnScreen pos
        this.acc = p5.Vector.sub(scrpos, this.pos);
        // Setting the magnitude of that vector
        this.acc.setMag(0.4);
         // Physics engine algorithm
        this.vel.add(this.acc);
        this.pos.add(this.vel);
        // Keep walker On Screen
        this.pos.x = constrain(this.pos.x, 0, width - 1);
        this.pos.y = constrain(this.pos.y, 0, height - 1);
    }
  }
  //  PARTICLE
  class Particle {
    constructor(startX, startY) {
      this.x = startX;
      this.y = startY;
      this.vx = random(-1,1);
      this.vy = random(-1,3);
      this.d = random(15, 35);
      this.acc = 0.1;
      this.alpha = 255;
    }
    update() {
      this.vy = this.vy - this.acc;
      this.x += this.vx;
      this.y += this.vy;
      this.alpha -= 4;
    }
    isFinished() {
      return this.alpha < 30;
    }
    show() {
      noStroke();
      fill(255, random(80,190), 0,this.alpha);
      circle(this.x, this.y, this.d)
    }
    show1(colorX, colorY){
      noStroke();
      fill(colorX, random(80,190), colorY, this.alpha);
      circle(this.x, this.y, this.d)
    }
    show2(colorX, colorY){
      noStroke();
      fill(random(80,190), colorX, colorY, this.alpha);
      circle(this.x, this.y, this.d)
    }
  }
  
  
  
  // SKETCH
  let particles = [];
  let particles2 = [];
  let particles3 = [];
  let walker, walker2, walker3;
  function setup() 
  {
    let canvas = createCanvas(windowWidth, windowHeight);
    canvas.parent('canvasDiv');
    //create walker
    walker = new Walker();
    walker2 = new Walker(); 
    walker3 = new Walker(); 
  
    if ('geolocation' in navigator) {
        console.log('geolocation available');
        navigator.geolocation.getCurrentPosition(position => {
          lat = position.coords.latitude;
          lon = position.coords.longitude;
          console.log(lat, lon);
          document.getElementById('latitude').textContent = lat;
          document.getElementById('longitude').textContent = lon;
        });
      } else {
        console.log('geolocation not available');
      }
  
  
  }
   
  function windowResized() {
      resizeCanvas(windowWidth, windowHeight);
  }
  function draw() 
  {
     
     // background(239,222,205);
      clear();
   
      walker.update();
      walker2.update();
      walker3.update();
      for (let i = 0; i < 2; i++) {
          let p = new Particle(walker.pos.x, walker.pos.y);
          particles.push(p);
      }
      for (let i = particles.length - 1; i >= 0; i--) {
          particles[i].update();
          particles[i].show();
          if (particles[i].isFinished()) {
              particles.splice(i, 1);
          }
      }  
      for (let i = 0; i < 2; i++) {
          let p2 = new Particle(walker2.pos.x, walker2.pos.y);
          particles2.push(p2);
      }
      for (let i = particles2.length - 1; i >= 0; i--) {
          particles2[i].update();
          particles2[i].show2(walker2.pos.x, walker2.pos.y);
          if (particles2[i].isFinished()) {
              particles2.splice(i, 1);
          }
      }
      for (let i = 0; i < 2; i++) {
          let p3 = new Particle(walker3.pos.x, walker3.pos.y);
          particles3.push(p3);
      }
      for (let i = particles3.length - 1; i >= 0; i--) {
          particles3[i].update();
          particles3[i].show1(walker3.pos.y, walker3.pos.x);
          if (particles3[i].isFinished()) {
              particles3.splice(i, 1);
          }
      }
     
  }